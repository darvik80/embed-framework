# Crearts IoT Platform Service — Implementation Prompt

## Goal

Build a self-hosted **IoT platform backend + dashboard** that speaks the device MQTT protocol defined in:

- `docs/iot-platform-mqtt-spec.md` (protocol **v1**)
- Device SDK: `components/crearts_iot/` (ESP-IDF)

Stack (fixed):

| Layer | Choice |
|-------|--------|
| API / MQTT ingest | **Go** (1.22+) |
| Persistence | **SQLite** (modernc.org/sqlite or mattn/go-sqlite3) |
| Dashboard | **React** (Vite + TypeScript) |
| Broker + Node-RED | `deploy/docker-compose.yml` (RabbitMQ MQTT + Node-RED) |


Deliver as a new top-level project (suggested path): `platform/` (or separate repo `crearts-iot-platform`). Do **not** put Go/React sources inside ESP-IDF `components/`.

---

## Context: MQTT contract (must implement)

Support **both** topic styles; payloads are identical.

**Full:** `iot/v1/{product_id}/{device_id}/{up\|down}/{capability}/{operation}`  
**Short:** `v1/t`, `v1/a`, `v1/r/req`, … (identity from MQTT `client_id` = `{product_id}.{device_id}`)

Capabilities to handle on the **server**:

| Up (device → platform) | Action |
|------------------------|--------|
| `status` / `v1/s` | Upsert online/offline, last_seen, LWT |
| `telemetry/data` / `v1/t` | Store time-series samples |
| `events/post` / `v1/e` | Store events/alarms |
| `attributes/report` / `v1/a` | Merge **reported** properties |
| `attributes/request` / `v1/a/req` | Reply on `…/attributes/response` / `v1/a/res` with `id` |
| `rpc/response` / `v1/r/res` | Complete pending server RPC |
| `rpc/request` / `v1/r/creq` | Optional client RPC (e.g. upload URL) |
| `ntp/request` / `v1/n/req` | Reply with server times + echo `id` |
| `ota/version`, `ota/query`, `ota/progress` | Firmware inventory / query / progress |
| `logs/report` / `v1/l` | Store device logs |

| Down (platform → device) | Action |
|--------------------------|--------|
| `attributes/update` / `v1/a/upd` | Push **desired** property changes |
| `rpc/request` / `v1/r/req` | Invoke method (`id` in JSON body) |
| `ota/update`, `ota/cancel` | OTA notify / cancel |
| `ntp/response` / `v1/n/res` | Time sync reply |

Correlation: JSON field `"id"` (uint32), **not** in the topic path. RPC success `code: 0`.

---

## Device authentication (required)

1. Operator registers device in dashboard → platform generates **access token**
2. Token shown **once**; stored hashed; RabbitMQ user provisioned (`user=token`, `pass=token`)
3. Device firmware uses `CreartsCredentials::createAccessToken(product, device, host, token)`
4. MQTT CONNECT: `username=token`, `password=token`, `client_id={product}.{device}`
5. Rotate token from dashboard invalidates the old broker user

Shared admin broker credentials are for platform ingest / lab only — not for devices.

---

## Architecture

```
┌─────────────┐   MQTT    ┌──────────────┐   AMQP/MQTT   ┌─────────────────┐
│ ESP devices │ ────────► │   RabbitMQ   │ ◄──────────── │  platform-api   │
│ crearts_iot │ ◄──────── │  :1883/15672 │               │  (Go)           │
└─────────────┘           └──────────────┘               │  - ingest       │
                                │                        │  - REST / WS    │
                                │ platform/v1/…          │  - RPC/OTA/NTP  │
                                ▼                        │  - event bridge │
                         ┌──────────────┐                │  - SQLite       │
                         │   Node-RED   │ ◄── REST ──────┘─────────────────┘
                         │  rules / CEP │
                         └──────────────┘
                                │
┌─────────────┐   HTTPS   ┌─────┴────────┐
│  Dashboard  │ ────────► │  React SPA   │  (+ link/iframe to Node-RED editor)
└─────────────┘           └──────────────┘
```

**Rule engine strategy:** do **not** build a full visual CEP inside Go/React. Use **Node-RED** as the automation/CEP layer, with the platform as the system of record and a stable **event/command bus** on RabbitMQ.

Recommended Go layout:

```
platform/
  cmd/platform/main.go
  internal/
    config/
    db/           # migrations, sqlite
    models/
    mqtt/         # ingest + publish down + rules bus bridge
    api/          # chi/echo/fiber REST
    ws/           # realtime to dashboard
    auth/
    services/     # devices, telemetry, attrs, rpc, ota, …
  web/            # React Vite app (embedded via embed.FS in prod)
  docker-compose.yml   # prefer extending ../deploy (rabbitmq + nodered)
  README.md
```

Single binary that:

1. Connects to RabbitMQ MQTT (or AMQP topic exchange `amq.topic`)
2. Serves REST + WebSocket
3. Serves React build from `embed.FS` (optional dual-mode: Vite dev proxy)
4. Bridges normalized device events to the **rules bus** for Node-RED (and accepts commands back)

---

## Data model (SQLite)

Keep schema simple and migration-based (`goose` / `golang-migrate` / embed SQL).

### Core

**products**

- `id`, `product_id` (unique slug), `name`, `description`
- `topic_style_default` (`short`|`full`)
- `created_at`, `updated_at`

**devices**

- `id`, `product_id` (FK), `device_id` (unique per product)
- `name`, `description`, `tags` (JSON)
- `access_token_hash` — hash of dashboard-issued MQTT access token (never store plaintext after create response)
- `token_prefix` — optional short prefix for support/debug (e.g. first 8 chars)
- `status` (`online`|`offline`|`unknown`)
- `last_seen_at`, `last_ip`, `fw_version`
- `topic_style` (`short`|`full`) — preferred downlink style
- `created_at`, `updated_at`

**device_credentials** (optional history)

- Token rotations; revoke old hashes

### Telemetry & properties

**telemetry_samples**

- `device_pk`, `key`, `value_num` / `value_text` / `value_bool`, `ts` (ms), `ingested_at`
- Indexes: `(device_pk, key, ts)`, retention job deletes older than N days

**device_attributes_reported** / **device_attributes_desired**

- `device_pk`, `key`, `value_json`, `updated_at`
- Desired changes enqueue MQTT `attributes/update`

**attribute_history** (optional)

- Audit of desired/reported changes

### Control plane

**rpc_calls**

- `id`, `device_pk`, `request_id`, `method`, `params_json`
- `status` (`pending`|`ok`|`error`|`timeout`)
- `code`, `message`, `data_json`, `created_at`, `completed_at`

**events**

- From `up/events/post`: `type`, `severity`, `code`, `message`, `data_json`, `ts`

**device_logs**

- `level`, `module`, `code`, `message`, `context_json`, `ts`

**ota_firmwares**

- `product_id`, `module`, `version`, `size`, `sha256`, `sign_method`, `sign`, `url`, `created_at`

**ota_jobs**

- `device_pk`, `firmware_id`, `status`, `step`, `desc`, `created_at`, `updated_at`

### Platform

**users** (dashboard)

- `email`, `password_hash`, `role` (`admin`|`operator`|`viewer`)

**api_tokens**

- Personal access tokens for REST automation

**audit_log**

- Who changed desired attrs / invoked RPC / created devices

**settings**

- Retention days, default RPC timeout (30s), platform display name

---

## Backend responsibilities

### MQTT ingest worker

1. Subscribe:
   - Full: `iot/v1/+/+/up/#`
   - Short: `v1/#` (attribute device via `client_id` / username mapping)
2. Parse topic → product/device/capability (or resolve short via session map)
3. Persist + emit internal events for WebSocket fan-out
4. Auto-create device? **No** by default — only known devices (secure). Optional “allow unknown” flag for lab mode.
5. Respond inline where needed (NTP, attribute request, client RPC)

**Short-topic identity:** resolve device by MQTT username (= access token) and/or `client_id` = `{product}.{device}`. Maintain `token → device` and `client_id → device` maps. Reject uplinks from unknown tokens.

Practical MVP:

- **Every device** gets a unique access token at dashboard registration
- MQTT CONNECT: `username=token`, `password=token` (or empty password if broker allows), `client_id=product.device`
- Platform syncs token to RabbitMQ as a user (`rabbitmqadmin` / management API) with topic ACL limited to that device
- Platform ingest uses a **service account** (not a device token) with read on `iot.v1.*.*.up.#` and `v1.#`
- Downlinks published using the device’s stored `topic_style`

### REST API (versioned `/api/v1`)

Auth: JWT (dashboard login) + optional Bearer API tokens.

**Products**

- CRUD products

**Devices**

- List/filter (status, product, search)
- **Create device** → generate access token → return **once** in API response + dashboard copy UI
  - Payload for firmware: `product_id`, `device_id`, `access_token`, broker host, topic style
  - Example snippet: `CreartsCredentials::createAccessToken(product, device, host, token, …)`
- Rotate token (invalidate old, show new once)
- Update metadata, disable device (revoke broker user)
- Delete
- Get detail: status, reported/desired, latest telemetry, recent events
- **Never** show full token again after create/rotate (only regenerate)

**Telemetry**

- Query series: `device`, `keys[]`, `from`, `to`, `limit`
- Latest values per key

**Attributes / properties**

- Get reported + desired
- Patch desired → write DB + MQTT `attributes/update`
- Optional: request refresh (`attributes/request`) and wait for response

**RPC**

- POST invoke `{ method, params, timeout_ms }` → allocate `id`, publish down, wait/poll
- List history

**Events / logs**

- List with filters (severity, level, time)

**OTA**

- Upload/register firmware metadata (URL or local file server)
- Start job / cancel / list progress

**Presence**

- Online count, last_seen

**Health**

- `/healthz`, `/readyz` (DB + MQTT connected)

### WebSocket / SSE

Push to dashboard:

- device status changes
- new telemetry (throttled)
- attribute updates
- RPC completed
- events / OTA progress

---

## Dashboard (React + TypeScript)

UI must be practical, not a generic AI “purple SaaS” template. Prefer a dense **ops console**: clear hierarchy, tables + detail drawers, charts for metrics.

### Pages / features (MVP → stretch)

| Area | MVP | Stretch |
|------|-----|---------|
| Login | yes | SSO later |
| Overview | online/offline counts, recent events, error rate | sparklines |
| Products | list/create/edit | JSON schema for telemetry keys |
| Devices | add, list, search, enable/disable, copy connection snippet | bulk import CSV |
| Device detail | status, metadata, connection info | map / notes |
| Metrics | time-series charts (recharts/u-plot), key picker | export CSV |
| Properties | reported (RO) + desired editor + save | diff / history |
| RPC console | method + JSON params + result | saved presets |
| Events | table + severity badges | ack/silence |
| Logs | filterable device logs | live tail |
| OTA | firmware list, push to device, progress | fleets / groups |
| Automation | link to Node-RED + status | embed editor, flow templates |
| Credentials | show host/port/client_id + token **only at create/rotate** | rotate |
| Settings | retention, timeouts | users/roles |
| Audit | — | who did what |

### Device “Add” flow

1. Select product (or create)
2. Enter `device_id`, display name, topic style
3. Platform generates **access token** (high entropy), hashes it, creates RabbitMQ MQTT user `username=token` / `password=token` with ACL for that device
4. Dashboard shows one-time panel:
   - Broker URI, `client_id`, **access token**, copy buttons
   - Firmware snippet using `CreartsCredentials::createAccessToken(...)`
5. Device appears `offline` until first retained/online `status`
6. Token cannot be retrieved later — only rotated

### Device monitoring

- Live online badge (WS)
- Last telemetry age
- Desired vs reported mismatch highlight
- Quick actions: Reboot RPC, Request attributes, Trigger OTA

---

## What else is required (do not skip)

Beyond “devices + metrics + properties”, the platform is incomplete without:

1. **Presence / LWT handling** — source of truth for online
2. **Products** as first-class (not only free-form device rows)
3. **Access-token MQTT auth** — token at registration; map token→device; provision/revoke broker users
4. **RPC** invoke + timeout + history
5. **Events & alarms** separate from telemetry
6. **NTP responder** (trivial, but in the device contract)
7. **OTA** metadata + job state (even if file hosting is external HTTPS)
8. **Log viewer** for field debugging
9. **Retention / cleanup** job for SQLite growth
10. **Realtime** dashboard channel (WS/SSE)
11. **Auth** on dashboard + RBAC at least admin/viewer
12. **Connection helper** UI (broker URI, topic style, client id)
13. **Health endpoints** + basic Prometheus metrics optional
14. **Idempotent ingest** / safe reconnect (devices QoS1 duplicates)
15. **Node-RED rules/CEP bus** — normalized events out, commands in (not a custom flow IDE)

---

## Config (env)

```
HTTP_ADDR=:8080
SQLITE_PATH=./data/platform.db
MQTT_BROKER=tcp://localhost:1883
MQTT_USERNAME=crearts
MQTT_PASSWORD=crearts
MQTT_CLIENT_ID=crearts-platform
JWT_SECRET=...
TELEMETRY_RETENTION_DAYS=30
RPC_DEFAULT_TIMEOUT_MS=30000
LAB_ALLOW_UNKNOWN_DEVICES=false
```

Compose: platform service `depends_on: rabbitmq` healthy; mount volume for SQLite.

---

## Implementation phases

### Phase 0 — Skeleton

- Go module, config, SQLite migrations, healthz
- React Vite app with router + auth shell
- docker-compose linking RabbitMQ

### Phase 1 — Devices + presence + telemetry

- Product/device CRUD
- MQTT ingest status + telemetry
- Dashboard: device list, detail, metric charts

### Phase 2 — Properties + RPC

- Reported/desired, push update
- RPC console + pending table
- Attribute request/response

### Phase 3 — Events, logs, NTP

- Ingest + UI tables
- NTP auto-reply

### Phase 4 — OTA + polish

- Firmware registry, jobs, progress
- Audit log, retention worker, WS hardening
- Embed SPA in Go binary

### Phase 5 — Rule engine / CEP (Node-RED)

- Rules-bus bridge (normalize uplink → `platform/v1/events/...`)
- Command ingress from Node-RED → device RPC / desired attributes
- `deploy/docker-compose.yml` (RabbitMQ + Node-RED); service MQTT user + example flow
- Dashboard: Automation page (open Node-RED, status, docs)
- Optional: simple built-in threshold alerts (email/webhook) without Node-RED for demos

---

## Acceptance criteria

- [ ] ESP device with `crearts_iot` connects to stack from `deploy/` and appears online in dashboard
- [ ] Telemetry from device shows on charts within ~1–2s (WS)
- [ ] Desired property change in UI reaches device (`v1/a/upd` or full topic)
- [ ] RPC reboot (or echo) round-trip works with `id` in body
- [ ] NTP request from device gets valid response
- [ ] Creating a device yields a one-time access token usable by `CreartsCredentials::createAccessToken`
- [ ] Device connects with token; unknown token is rejected
- [ ] Token rotate invalidates the previous credential
- [ ] SQLite survives restart; retention does not explode disk in lab
- [ ] README: run rabbitmq, run platform, flash device, screenshots/API examples
- [ ] Node-RED receives a telemetry event on the rules bus and can invoke device RPC / set desired attribute via command topic or REST

---

## Non-goals (MVP)

- Multi-tenant SaaS billing
- Kubernetes operators
- Replacing RabbitMQ with an in-process broker
- Mobile apps
- Reimplementing a full visual flow editor inside the React dashboard (use Node-RED instead)

---

## Rule engine / CEP (Node-RED)

### Decision

| Approach | Role |
|----------|------|
| **Node-RED** (required integration) | Visual flows, CEP-style sequences, timers, HTTP/email/Slack, multi-device logic |
| **Platform Go** | Device registry, auth, persistence, MQTT protocol, command execution, audit |
| **Optional lite rules in Go** | Simple threshold → event/webhook only; not a substitute for Node-RED |

Do not invent a competing drag-and-drop engine in React for MVP.

### Event / command bus (RabbitMQ topics)

Platform republishes **normalized** JSON (not raw device short topics) so Node-RED never needs device access tokens.

**Events (platform → rules):**

```
platform/v1/events/{product_id}/{device_id}/status
platform/v1/events/{product_id}/{device_id}/telemetry
platform/v1/events/{product_id}/{device_id}/attributes/reported
platform/v1/events/{product_id}/{device_id}/attributes/desired
platform/v1/events/{product_id}/{device_id}/event
platform/v1/events/{product_id}/{device_id}/rpc/result
platform/v1/events/{product_id}/{device_id}/ota
platform/v1/events/{product_id}/{device_id}/log
```

Example telemetry envelope:

```json
{
  "ts": 1451649600512,
  "product_id": "esp32-cam",
  "device_id": "cam-001",
  "keys": { "temperature": 22.5, "humidity": 61 }
}
```

**Commands (rules → platform → device):**

```
platform/v1/commands/{product_id}/{device_id}/rpc
platform/v1/commands/{product_id}/{device_id}/attributes/desired
platform/v1/commands/{product_id}/{device_id}/ota
```

RPC command body (platform allocates device-facing `id`):

```json
{
  "method": "reboot",
  "params": { "delayMs": 1000 },
  "timeout_ms": 30000,
  "correlation_id": "nr-flow-1"
}
```

Desired attributes command:

```json
{
  "targetTemperature": 26,
  "enabled": true
}
```

Platform validates product/device, enforces RBAC/service credentials, executes via normal downlink path, writes audit_log.

Alternatively Node-RED may call REST (`POST /api/v1/devices/.../rpc`) with an API token — support **both**; MQTT bus preferred for flow latency.

### Node-RED deployment

- Compose: `deploy/docker-compose.yml` (RabbitMQ + Node-RED on one network)
- MQTT user e.g. `nodered` / secret with ACL:
  - read `platform.v1.events.#`
  - write `platform.v1.commands.#`
- Persist `/data` volume for flows
- Editor: `http://localhost:1880`
- Ship **example flows**: threshold on temperature → RPC or desired attr; offline alert → webhook

### Dashboard UX

- **Automation** nav item → opens Node-RED (new tab) and short “how to wire” docs
- Optional iframe embed behind login (same-origin reverse proxy)
- Show bridge health: last event forwarded, command errors

### Security

- Node-RED must **not** use device access tokens
- Only the platform service account talks device `up`/`down` topics
- Commands from Node-RED authenticated as the `nodered` MQTT user or API token
- Rate-limit command ingress

### Config additions

```
RULES_BUS_ENABLED=true
RULES_BUS_PREFIX=platform/v1
NODERED_URL=http://localhost:1880
```

---

## References

- MQTT spec: `docs/iot-platform-mqtt-spec.md`
- Device SDK: `components/crearts_iot/README.md`
- Stack: `deploy/README.md` (`docker compose up` → RabbitMQ + Node-RED)
- Inspired UX: ThingsBoard device pages (list, telemetry, attributes, RPC) — but follow **our** topic/payload contract, not TB APIs
- Automation UX: Node-RED editor for flows/CEP; platform owns devices and command execution
