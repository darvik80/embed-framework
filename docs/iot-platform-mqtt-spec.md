# IoT Platform MQTT Specification

**Protocol version:** `v1`  
**Broker:** MQTT 3.1.1 / 5.0 (RabbitMQ MQTT plugin or any compliant broker)

This document defines a standardized MQTT protocol for IoT device communication. The design consolidates strengths of Alibaba Cloud IoT and ThingsBoard while fixing gaps common in both (presence, versioning, OTA security, consistent time/correlation).

## Design Principles

1. **Versioned**: Protocol version lives in the topic (`v1` segment / short prefix).
2. **Two topic styles**: **Full** (routable, identity in path) and **Short** (ThingsBoard-like, identity from MQTT session).
3. **Product-centric identity**: Full topics use stable `product_id`; short topics rely on client credentials / client id.
4. **Bidirectional**: Full style uses explicit `up`/`down`; short style encodes direction in the mnemonic.
5. **Correlation in body**: Request/response pairs carry `id` in the JSON payload. Topics stay stable (no per-request leaf).
6. **Flat JSON payloads**: No Alink-style envelope. Payload is the data (identical for both topic styles).
7. **QoS-aware**: Explicit QoS per capability; critical paths use QoS 1.
8. **Extensible**: New capabilities add leaf segments (or short codes) without breaking existing clients.

## Topic Structure

Both styles share the same payloads, QoS rules, and `id` correlation. A device or server picks one style per connection (do not mix publish styles on one client).

### Topic styles

| Style | When to use | Identity |
|-------|-------------|----------|
| **Full** | Gateways, multi-tenant routing, server-side fan-in, ACL by path | `{product_id}/{device_id}` in topic |
| **Short** | Constrained devices (less flash/RAM for topic strings), TB-like DX | From MQTT client id / credentials |

Default for new device firmware: **Short**. Default for brokers/ingest/docs examples: **Full** (self-describing).

### Full pattern

```
iot/v1/{product_id}/{device_id}/{direction}/{capability}/{operation}
```

| Segment | Description | Example |
|---------|-------------|---------|
| `v1` | Protocol major version | `v1` |
| `product_id` | Stable product / model key (ACL, routing) | `esp32-cam` |
| `device_id` | Unique device within product | `cam-001` |
| `direction` | `up` or `down` | `up` |
| `capability` | Functional domain | `telemetry`, `attributes`, `rpc`, … |
| `operation` | Action within capability | `data`, `request`, `update` |

### Short pattern (ThingsBoard-inspired)

```
v1/{code}[/{op}]
```

No product/device in the path. The broker/platform resolves the device from the MQTT session: `client_id` = `{product_id}.{device_id}` and/or the access token used as MQTT username.

| Short | Dir | Full equivalent |
|-------|-----|-----------------|
| `v1/s` | up | `…/up/status` |
| `v1/t` | up | `…/up/telemetry/data` |
| `v1/e` | up | `…/up/events/post` |
| `v1/a` | up | `…/up/attributes/report` |
| `v1/a/req` | up | `…/up/attributes/request` |
| `v1/a/res` | down | `…/down/attributes/response` |
| `v1/a/upd` | down | `…/down/attributes/update` |
| `v1/r/req` | down | `…/down/rpc/request` |
| `v1/r/res` | up | `…/up/rpc/response` |
| `v1/r/creq` | up | `…/up/rpc/request` (client RPC) |
| `v1/r/cres` | down | `…/down/rpc/response` (client RPC) |
| `v1/n/req` | up | `…/up/ntp/request` |
| `v1/n/res` | down | `…/down/ntp/response` |
| `v1/o/ver` | up | `…/up/ota/version` |
| `v1/o/q` | up | `…/up/ota/query` |
| `v1/o/upd` | down | `…/down/ota/update` |
| `v1/o/can` | down | `…/down/ota/cancel` |
| `v1/o/p` | up | `…/up/ota/progress` |
| `v1/l` | up | `…/up/logs/report` |

Mnemonic: `s` status, `t` telemetry, `e` events, `a` attributes, `r` RPC, `n` NTP, `o` OTA, `l` logs — same idea as ThingsBoard `v2/t`, `v2/a`, `v2/r/...`.

**Short vs ThingsBoard:** attribute *updates* use `v1/a/upd` (not the same topic as report). Correlation stays in JSON `id` (not in the topic).

### Correlation (`id`)

All request/response operations include a numeric `id` in the **JSON body** (uint32). The response echoes the same `id`.

- Topics are fixed strings — easier ACL, routing, and device subscribe lists.
- Callers may have multiple in-flight requests; match responses by `id`.
- `id` is scoped per direction/initiator (device-generated for up-requests, server-generated for down-requests).
- Do **not** put `id` in the topic path.

**Identity rules**
- `product_id` and `device_id`: `[a-zA-Z0-9][a-zA-Z0-9._-]{0,62}` (max 63 chars each).
- No `/` in identifiers.
- Client ID: `{product_id}.{device_id}` (required for short style; must match full-topic identity when using full style).

**Multi-tenant (optional prefix)**  
SaaS deployments may prepend a tenant:

```
iot/v1/{tenant_id}/{product_id}/{device_id}/...
```

Devices in single-tenant / self-hosted setups omit `tenant_id`. Server ACL and routing must agree on which form is used.

**Gateway / sub-devices (optional)**

```
iot/v1/{product_id}/{gateway_id}/up/gateway/proxy/{sub_product}/{sub_device_id}/...
```

The gateway publishes/subscribes on behalf of children; leaf capability paths are otherwise identical.

---

## Presence / Lifecycle

Presence is owned by the **broker + platform**, not by application publishes from the device.

| Signal | Source |
|--------|--------|
| **Online** | MQTT session established (platform / RabbitMQ connection tracking). Device does **not** publish an online status message. |
| **Offline (unclean)** | MQTT **LWT** on the status topic (broker publishes retained payload). |
| **Offline (clean)** | Platform observes disconnect / session end (LWT may not fire on clean disconnect). |

### Status topic (LWT only)

```
iot/v1/{product_id}/{device_id}/up/status
```
Short: `v1/s`

**LWT payload (CONNECT will, retained, QoS 1)** — published by the broker when the client drops uncleanly:

```json
{
  "online": false,
  "ts": 0,
  "reason": "lwt"
}
```

| Field | Type | Notes |
|-------|------|--------|
| `online` | bool | always `false` in LWT |
| `ts` | number (ms) | may be 0 |
| `reason` | string | `lwt` |

**Device CONNECT must set:**
- Will topic: `…/up/status` or `v1/s` (same topic style as the session)
- Will payload: offline JSON above
- Retain: true
- QoS: 1

Firmware / metadata (fw version, IP) belong in **attributes** or telemetry — not in a fake online status publish.

---

## Telemetry (Metrics)

Continuous time-series data.

### Device → Server

```
iot/v1/{product_id}/{device_id}/up/telemetry/data
```
Short: `v1/t`

**Formats**

1. **Simple KV** (server stamps receive time):
```json
{
  "temperature": 22.5,
  "humidity": 61,
  "cpuUsage": 45
}
```

2. **Client timestamp**:
```json
{
  "ts": 1451649600512,
  "values": {
    "temperature": 22.5,
    "humidity": 61
  }
}
```

3. **Batch**:
```json
[
  {"ts": 1451649600512, "values": {"temperature": 22.5}},
  {"ts": 1451649601512, "values": {"temperature": 23.0}}
]
```

**QoS:** 1 default; QoS 0 allowed for high-rate / loss-tolerant metrics (document per product).  
**Retain:** false

---

## Events

Discrete, non-periodic occurrences (alarms, button press, state transitions). Not a substitute for telemetry.

### Device → Server

```
iot/v1/{product_id}/{device_id}/up/events/post
```
Short: `v1/e`

```json
{
  "ts": 1451649600512,
  "id": "evt-7f3a",
  "type": "alarm",
  "severity": "critical",
  "code": "TEMP_HIGH",
  "message": "Temperature above threshold",
  "data": {
    "temperature": 87.2,
    "threshold": 80
  }
}
```

| Field | Required | Notes |
|-------|----------|--------|
| `ts` | yes | epoch ms |
| `type` | yes | `alarm`, `info`, `lifecycle`, product-defined |
| `code` | yes | stable machine code |
| `id` | no | idempotency / dedup |
| `severity` | no | `info`, `warning`, `critical` |
| `message` | no | human-readable |
| `data` | no | structured context |

**QoS:** 1

---

## Attributes (Properties)

Two scopes:
- **reported** — device-owned state (firmware, serial, last wifi rssi).
- **desired** — server-owned config pushed to the device (`targetTemperature`, `enabled`).

### Report reported attributes (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/attributes/report
```
Short: `v1/a`

```json
{
  "firmwareVersion": "2.1.0",
  "serialNumber": "SN-4A21F",
  "model": "ESP32-CAM"
}
```

### Request attributes (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/attributes/request
```
Short: `v1/a/req`

```json
{
  "id": 7,
  "reported": ["firmwareVersion", "serialNumber"],
  "desired": ["targetTemperature", "enabled"]
}
```

Empty array or omitted key means “do not fetch that scope”. To fetch all keys of a scope, send `["*"]`.

### Attribute response (Server → Device)

```
iot/v1/{product_id}/{device_id}/down/attributes/response
```
Short: `v1/a/res`

```json
{
  "id": 7,
  "reported": {
    "firmwareVersion": "2.1.0",
    "serialNumber": "SN-4A21F"
  },
  "desired": {
    "targetTemperature": 25,
    "enabled": true
  }
}
```

### Desired attribute update (Server → Device)

```
iot/v1/{product_id}/{device_id}/down/attributes/update
```
Short: `v1/a/upd`

Flat object of changed desired keys only:
```json
{
  "targetTemperature": 26,
  "enabled": false
}
```

Device should apply changes and re-`report` any mirrored reported keys if applicable.

**QoS:** 1

---

## RPC (Remote Procedure Call)

### Server → Device request

```
iot/v1/{product_id}/{device_id}/down/rpc/request
```
Short: `v1/r/req`

```json
{
  "id": 42,
  "method": "reboot",
  "params": {
    "delayMs": 5000
  }
}
```

### Device → Server response

```
iot/v1/{product_id}/{device_id}/up/rpc/response
```
Short: `v1/r/res`

**Success:**
```json
{
  "id": 42,
  "code": 0,
  "message": "ok",
  "data": {
    "rebooting": true
  }
}
```

**Error:**
```json
{
  "id": 42,
  "code": 404,
  "message": "unknown method",
  "data": null
}
```

### Device → Server request (optional client RPC)

```
iot/v1/{product_id}/{device_id}/up/rpc/request
```
Short: `v1/r/creq`

```json
{
  "id": 9,
  "method": "issueUploadUrl",
  "params": {
    "contentType": "image/jpeg"
  }
}
```

### Server → Device response

```
iot/v1/{product_id}/{device_id}/down/rpc/response
```
Short: `v1/r/cres`

Same response shape (`id`, `code`, `message`, `data`).

### RPC status codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `400` | Bad params |
| `404` | Unknown method |
| `408` | Handler timeout |
| `409` | Busy / conflicting state |
| `500` | Internal device/server error |
| `501` | Not implemented |

Default RPC timeout: **30 s** (both directions). Caller treats missing response as `408`.

**QoS:** 1 request, 1 response

---

## NTP (Time Synchronization)

### Device → Server

```
iot/v1/{product_id}/{device_id}/up/ntp/request
```
Short: `v1/n/req`

```json
{
  "id": 3,
  "deviceSendTime": 1451649600000
}
```

### Server → Device

```
iot/v1/{product_id}/{device_id}/down/ntp/response
```
Short: `v1/n/res`

```json
{
  "id": 3,
  "deviceSendTime": 1451649600000,
  "serverRecvTime": 1451649600010,
  "serverSendTime": 1451649600015
}
```

All times are **epoch milliseconds (number)**. Echo `id` from the request.

```
RTT = deviceRecvTime - deviceSendTime
serverTime ≈ serverSendTime + RTT / 2
```

**QoS:** 1

---

## OTA (Over-The-Air Updates)

### Report firmware version (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/ota/version
```
Short: `v1/o/ver`

```json
{
  "version": "2.1.0",
  "module": "main"
}
```

### Query update (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/ota/query
```
Short: `v1/o/q`

```json
{
  "module": "main",
  "version": "2.1.0"
}
```

### Update notification (Server → Device)

```
iot/v1/{product_id}/{device_id}/down/ota/update
```
Short: `v1/o/upd`

```json
{
  "version": "2.2.0",
  "module": "main",
  "size": 1048576,
  "url": "https://cdn.example.com/firmware/v2.2.0.bin",
  "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  "signMethod": "sha256-rsa",
  "sign": "base64-signature…",
  "force": false
}
```

| Field | Required | Notes |
|-------|----------|--------|
| `version` | yes | target version |
| `module` | yes | e.g. `main`, `modem` |
| `size` | yes | bytes |
| `url` | yes* | HTTPS download (*or `stream: "mqtt"` for chunked) |
| `sha256` | yes | hex digest of image |
| `signMethod` | recommended | `sha256`, `sha256-rsa`, `sha256-ecdsa` |
| `sign` | recommended | signature over image (or over sha256, documented per product) |
| `force` | no | skip “same version” guard |

**Transport modes**
- **HTTPS** (`url`) — default.
- **MQTT stream** — set `"stream": "mqtt"` and omit `url`; chunks arrive on `down/ota/chunk/{offset}` (product-specific; optional extension).

### Cancel (Server → Device)

```
iot/v1/{product_id}/{device_id}/down/ota/cancel
```
Short: `v1/o/can`

```json
{
  "module": "main",
  "reason": "superseded"
}
```

### Progress / result (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/ota/progress
```
Short: `v1/o/p`

```json
{
  "module": "main",
  "step": 50,
  "desc": "Downloading firmware"
}
```

`step` semantics:
- `0`…`100` — percent progress
- `-1` — download failed
- `-2` — checksum/signature failed
- `-3` — install/flash failed
- `-4` — cancelled
- `101` — success / done (optional; some devices reboot before sending)

**QoS:** 1

---

## Device Logs

### Device → Server

```
iot/v1/{product_id}/{device_id}/up/logs/report
```
Short: `v1/l`

```json
[
  {
    "ts": 1705312200000,
    "level": "ERROR",
    "module": "wifi",
    "code": "WIFI_DISCONNECT",
    "message": "Connection lost",
    "context": {
      "ssid": "MyNetwork"
    }
  }
]
```

All timestamps are epoch **ms numbers** (same as telemetry/events).

**QoS:** 0 default; QoS 1 recommended for `ERROR` / `FATAL` batches.

---

## Error Response Shape

All response payloads that indicate failure include `id` and:

```json
{
  "id": 42,
  "code": 400,
  "message": "human-readable reason",
  "data": null
}
```

Attribute success responses use scopes (`reported` / `desired`) plus `id`. RPC/NTP always include the fields specified in their sections.

---

## Connection & Authentication

### Recommended MQTT settings

| Setting | Value |
|---------|--------|
| Port | 1883 (plain), 8883 (TLS) |
| Keep-alive | 60 s |
| Clean session | `false` (persistent session) for production devices |
| Auto-reconnect | exponential backoff (cap ~5 min) |
| Max inflight | ≥ 1; prefer ≥ 4 if batching |

### Authentication

Devices authenticate with an **access token** issued by the platform when the device is registered in the dashboard. Shared broker admin passwords must not be used on devices.

| MQTT CONNECT field | Value |
|--------------------|--------|
| `username` | `<access_token>` (opaque secret from dashboard) |
| `password` | empty string, **or** the same `<access_token>` if the broker rejects empty passwords (RabbitMQ default) |
| `client_id` | `{product_id}.{device_id}` (required; used for short-topic identity and ACL) |

**Rules**
1. Dashboard **Create device** generates a high-entropy token, shows it once (copy to firmware), stores only a hash (or broker-side secret) server-side.
2. Token uniquely maps to one `(product_id, device_id)`. Rotate = invalidate old token, issue new.
3. Broker/platform MUST reject unknown tokens and MUST scope ACL to that device’s topics.
4. Optional later: client certificates in addition to token. Not a substitute for registration in MVP.

**Lab-only:** a shared MQTT user may be used for bring-up tests; it is not the production auth model.

### Client ID

```
{product_id}.{device_id}
```

Example: `esp32-cam.cam-001`

Broker ACL should restrict the device to its own topics:

**Full style:**
```
iot/v1/{product_id}/{device_id}/#
```

**Short style:**
```
v1/#
```
(plus bind the session to that device via client id / credentials — short topics are not cross-device safe without session ACL)

---

## Subscription Strategy

### Device (on CONNECT)

**Full:**
```
iot/v1/{product_id}/{device_id}/down/#
```

**Short** (one wildcard; session-scoped):
```
v1/#
```

Or subscribe only to down mnemonics if ACL cannot isolate the session:
`v1/a/res`, `v1/a/upd`, `v1/r/req`, `v1/r/cres`, `v1/n/res`, `v1/o/upd`, `v1/o/can`.

Configure LWT on the status topic before CONNECT (same topic style). Do **not** publish online/offline status from application code.

### Server / ingest

**Full:**
```
iot/v1/+/+/up/#
```

With tenant prefix:
```
iot/v1/+/+/+/up/#
```

**Short:**
```
v1/#
```
Platform must attach `product_id` / `device_id` from the MQTT connection (client id or auth token) before routing/storage. Reject short-topic publishes that cannot be attributed to a device.

---

## Time & Payload Conventions

| Rule | Convention |
|------|------------|
| Timestamps | Epoch **milliseconds**, JSON **number** |
| Booleans | JSON boolean, not `0`/`1` strings |
| Null | use JSON `null` or omit optional fields |
| Numbers | JSON number (not numeric strings) |
| Strings | UTF-8 |
| Request IDs | JSON number field `id` (uint32); echoed in response |
| Compression | Optional; negotiate out-of-band (not in v1 topic) |

---

## Comparison with Existing Platforms

| Feature | ThingsBoard | Alibaba Cloud | This Spec |
|---------|-------------|---------------|-----------|
| Topic structure | `v2/t`, `v2/a` | `/sys/{product}/{device}/...` | Full `iot/v1/{product}/{device}/…` **and** Short `v1/t`, `v1/a`, … |
| Protocol version | implicit | implicit | in path / short prefix |
| Presence / LWT | custom | limited | session online + LWT offline on `up/status` / `v1/s` |
| RPC | `v2/r/req/+` | `/thing/service/+/+` | `down/rpc/request` / `v1/r/req` + `id` in body |
| Attributes | `v2/a` | property post | reported/desired (`v1/a`, `v1/a/upd`) |
| Events | telemetry mix | event post | `up/events/post` / `v1/e` |
| NTP | not built-in | `/ext/ntp/...` | `up/ntp/request` / `v1/n/req` |
| OTA | not built-in | `/ota/device/...` | sha256 + sign + cancel + step codes |
| Request ID | in topic | in JSON | **in JSON (`id`)** |
| Payload | flat JSON | Alink envelope | **flat JSON only** |

---

## Best Practices

1. QoS 1 for status, attributes, RPC, OTA, events; QoS 0 only where loss is acceptable.
2. Batch telemetry to cut MQTT overhead.
3. Configure LWT on status topic; treat platform/broker session as online — do not publish online status from the device.
4. Enforce RPC timeout (30 s) and map to code `408`.
5. Validate JSON before publish; reject unknown RPC methods with `404`.
6. Verify OTA `sha256` and `sign` before flash.
7. Keep topic strings in flash via a single topic-builder helper.
8. Prefer `desired` updates for config; avoid stuffing config into telemetry.

---

## Broker Notes (RabbitMQ MQTT plugin)

MQTT topic semantics are the contract. Broker topology is an ops concern.

With RabbitMQ MQTT plugin, MQTT topics map to AMQP topic-exchange routing keys (`/` → `.`). Recommended:

```
Exchange: amq.topic (or dedicated iot.mqtt, type topic)

Ingest queue: iot.up
  Bind: iot.v1.*.*.up.#

Per-device down is handled by the MQTT plugin session;
  application publishes to MQTT topic:
  iot/v1/{product}/{device}/down/...
```

Optional DLX for poison ingest messages:
```
Exchange: iot.dlx (topic)
Queue: iot.dead.letters
```

Do **not** use a direct exchange for device downlink when speaking MQTT — publish MQTT topics and let the plugin deliver to the connected client.

---

## Implementation Checklist

- [ ] Assign `product_id` / `device_id`; lock ACL to `iot/v1/{product}/{device}/#`
- [ ] Credentials provider + client id `{product}.{device}`
- [ ] Topic builder with `TopicStyle::Full` and `TopicStyle::Short`
- [ ] LWT on status topic (`…/up/status` or `v1/s`); no app-level online/offline publish
- [ ] Telemetry / events / attributes / RPC builders (`id` in body)
- [ ] Subscribe `down/#` on connect
- [ ] RPC handler registry + 30 s timeout + echo `id`
- [ ] NTP with body `id` + numeric timestamps
- [ ] OTA state machine (query, update, progress, cancel, verify sha256/sign)
- [ ] Logs reporter
- [ ] Metrics → telemetry bridge
- [ ] Broker ACL + ingest bindings
- [ ] Mock broker integration tests
- [ ] Deploy and monitor connection / message rate / DLQ

---

## Migration

### From ThingsBoard

| Old | New |
|-----|-----|
| `v2/t` | `iot/v1/{product}/{id}/up/telemetry/data` |
| `v2/a` | `iot/v1/{product}/{id}/up/attributes/report` |
| `v2/a` (push) | `…/down/attributes/update` |
| `v2/a/req/{id}` | `…/up/attributes/request` + `"id"` in JSON |
| `v2/r/req/{id}` | `…/down/rpc/request` + `"id"` in JSON |
| `v2/r/res/{id}` | `…/up/rpc/response` + `"id"` in JSON |
| `clientKeys` CSV string | `reported` / `desired` string arrays |
| RPC HTTP-like `200` | `code: 0` success |

### From Alibaba Cloud

| Old | New |
|-----|-----|
| `/sys/{product}/{device}/thing/event/property/post` | `…/up/attributes/report` |
| `/sys/.../thing/service/{method}` | `…/down/rpc/request` + `method` / `id` in JSON |
| `/ext/ntp/...` | `…/up/ntp/request` + `id` in JSON |
| `/ota/device/inform/...` | `…/up/ota/version` |
| Alink envelope | strip; use flat JSON with `id` where needed |
| `productKey` | `product_id` |

### From previous draft of this spec (`device_type`, no version)

| Old | New |
|-----|-----|
| `iot/{device_type}/{id}/...` | `iot/v1/{product_id}/{id}/...` |
| `…/rpc/request/42` (id in topic) | `…/rpc/request` + `"id": 42` in body |
| `up/ntp/request/{request_id}` | `up/ntp/request` + `"id"` in body |
| optional envelope | removed |
| OTA `md5` | `sha256` (+ `sign` / `signMethod`) |
| attribute `clientKeys`/`sharedKeys` CSV | `reported`/`desired` arrays |
| RPC success `code: 200` | `code: 0` |

---

## Appendix: Topic Map

Device: `product_id=esp32-cam`, `device_id=cam-001`  
Client id: `esp32-cam.cam-001`

| Capability | Full | Short |
|------------|------|-------|
| Status (LWT only) | `iot/v1/esp32-cam/cam-001/up/status` | `v1/s` |
| Telemetry | `…/up/telemetry/data` | `v1/t` |
| Events | `…/up/events/post` | `v1/e` |
| Attr report | `…/up/attributes/report` | `v1/a` |
| Attr request | `…/up/attributes/request` | `v1/a/req` |
| Attr response | `…/down/attributes/response` | `v1/a/res` |
| Attr update | `…/down/attributes/update` | `v1/a/upd` |
| RPC request | `…/down/rpc/request` | `v1/r/req` |
| RPC response | `…/up/rpc/response` | `v1/r/res` |
| Client RPC req | `…/up/rpc/request` | `v1/r/creq` |
| Client RPC res | `…/down/rpc/response` | `v1/r/cres` |
| NTP req / res | `…/up/ntp/request` ↔ `…/down/ntp/response` | `v1/n/req` ↔ `v1/n/res` |
| OTA version | `…/up/ota/version` | `v1/o/ver` |
| OTA query | `…/up/ota/query` | `v1/o/q` |
| OTA update | `…/down/ota/update` | `v1/o/upd` |
| OTA cancel | `…/down/ota/cancel` | `v1/o/can` |
| OTA progress | `…/up/ota/progress` | `v1/o/p` |
| Logs | `…/up/logs/report` | `v1/l` |
| Device sub | `…/down/#` | `v1/#` |

Correlation example (RPC, either style): body `{"id":42,"method":"reboot","params":{...}}` on request topic; response body `{"id":42,"code":0,...}` on response topic.

