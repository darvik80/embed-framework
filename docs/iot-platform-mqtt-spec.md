# IoT Platform MQTT Specification

**Protocol version:** `v1`  
**Broker:** MQTT 3.1.1 / 5.0 (RabbitMQ MQTT plugin or any compliant broker)

This document defines a standardized MQTT protocol for IoT device communication. The design consolidates strengths of Alibaba Cloud IoT and ThingsBoard while fixing gaps common in both (presence, versioning, OTA security, consistent time/correlation).

## Design Principles

1. **Versioned**: Protocol version lives in the topic path (`iot/v1/...`) so clients can evolve without silent breakage.
2. **Product-centric identity**: Topics use stable `product_id` (not a free-form “sensor/camera” label).
3. **Bidirectional**: Clear `up` (device→server) and `down` (server→device) separation.
4. **Single correlation model**: Request IDs live in the topic path only — never duplicated in an optional envelope.
5. **Flat JSON payloads**: No Alink-style envelope. Payload is the data.
6. **QoS-aware**: Explicit QoS per capability; critical paths use QoS 1.
7. **Extensible**: New capabilities add leaf segments without breaking existing subscriptions.

## Topic Structure

### Base Pattern

```
iot/v1/{product_id}/{device_id}/{direction}/{capability}/{operation}[/{request_id}]
```

| Segment | Description | Example |
|---------|-------------|---------|
| `v1` | Protocol major version | `v1` |
| `product_id` | Stable product / model key (ACL, routing) | `esp32-cam` |
| `device_id` | Unique device within product | `cam-001` |
| `direction` | `up` or `down` | `up` |
| `capability` | Functional domain | `telemetry`, `attributes`, `rpc`, … |
| `operation` | Action within capability | `data`, `request`, `update` |
| `request_id` | Correlation id (decimal string) when needed | `42` |

**Identity rules**
- `product_id` and `device_id`: `[a-zA-Z0-9][a-zA-Z0-9._-]{0,62}` (max 63 chars each).
- No `/` in identifiers.
- Client ID: `{product_id}.{device_id}` (must match topic identity).

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

Required for monitoring and stale-session cleanup.

### Device → Server (publish, retained)

```
iot/v1/{product_id}/{device_id}/up/status
```

**Online (after connect, retained, QoS 1):**
```json
{
  "online": true,
  "ts": 1451649600512,
  "fw": "2.1.0",
  "ip": "192.168.1.10"
}
```

**Offline via LWT (broker publishes on unclean disconnect, retained, QoS 1):**
```json
{
  "online": false,
  "ts": 0,
  "reason": "lwt"
}
```

**Graceful offline (device publishes before disconnect, retained, QoS 1):**
```json
{
  "online": false,
  "ts": 1451649600999,
  "reason": "shutdown"
}
```

| Field | Type | Notes |
|-------|------|--------|
| `online` | bool | required |
| `ts` | number (ms) | device clock; may be 0 in LWT if clock unknown |
| `reason` | string | `lwt`, `shutdown`, `reboot`, optional |
| `fw` | string | optional on online |
| `ip` | string | optional |

**LWT setup (device MQTT CONNECT):**
- Topic: `…/up/status`
- Payload: offline LWT JSON above
- Retain: true
- QoS: 1

---

## Telemetry (Metrics)

Continuous time-series data.

### Device → Server

```
iot/v1/{product_id}/{device_id}/up/telemetry/data
```

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

```json
{
  "firmwareVersion": "2.1.0",
  "serialNumber": "SN-4A21F",
  "model": "ESP32-CAM"
}
```

### Request attributes (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/attributes/request/{request_id}
```

```json
{
  "reported": ["firmwareVersion", "serialNumber"],
  "desired": ["targetTemperature", "enabled"]
}
```

Empty array or omitted key means “do not fetch that scope”. To fetch all keys of a scope, send `["*"]`.

### Attribute response (Server → Device)

```
iot/v1/{product_id}/{device_id}/down/attributes/response/{request_id}
```

```json
{
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
iot/v1/{product_id}/{device_id}/down/rpc/request/{request_id}
```

```json
{
  "method": "reboot",
  "params": {
    "delayMs": 5000
  }
}
```

### Device → Server response

```
iot/v1/{product_id}/{device_id}/up/rpc/response/{request_id}
```

**Success:**
```json
{
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
  "code": 404,
  "message": "unknown method",
  "data": null
}
```

### Device → Server request (optional client RPC)

```
iot/v1/{product_id}/{device_id}/up/rpc/request/{request_id}
```

```json
{
  "method": "issueUploadUrl",
  "params": {
    "contentType": "image/jpeg"
  }
}
```

### Server → Device response

```
iot/v1/{product_id}/{device_id}/down/rpc/response/{request_id}
```

Same response shape as device→server RPC response.

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
iot/v1/{product_id}/{device_id}/up/ntp/request/{request_id}
```

```json
{
  "deviceSendTime": 1451649600000
}
```

### Server → Device

```
iot/v1/{product_id}/{device_id}/down/ntp/response/{request_id}
```

```json
{
  "deviceSendTime": 1451649600000,
  "serverRecvTime": 1451649600010,
  "serverSendTime": 1451649600015
}
```

All times are **epoch milliseconds (number)**.

```
RTT = deviceRecvTime - deviceSendTime
serverTime ≈ serverSendTime + RTT / 2
```

**QoS:** 1  
Correlate via `request_id` in the topic (required).

---

## OTA (Over-The-Air Updates)

### Report firmware version (Device → Server)

```
iot/v1/{product_id}/{device_id}/up/ota/version
```

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

All `down/.../response/{request_id}` payloads that indicate failure use:

```json
{
  "code": 400,
  "message": "human-readable reason",
  "data": null
}
```

Success may omit `code` only for attribute responses (object scopes); RPC/NTP always include explicit fields as specified above.

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

1. Username/password (simple / lab)
2. Client certificate (TLS, production)
3. Token (JWT / OAuth2) as MQTT password

### Client ID

```
{product_id}.{device_id}
```

Example: `esp32-cam.cam-001`

Broker ACL should restrict publish/subscribe to:
```
iot/v1/{product_id}/{device_id}/#
```

---

## Subscription Strategy

### Device (on CONNECT)

```
iot/v1/{product_id}/{device_id}/down/#
```

Covers RPC, attributes, NTP, OTA, client-RPC responses.

Also publish retained online status to `up/status` after connect; configure LWT before CONNECT.

### Server / ingest

```
iot/v1/+/+/up/#
```

With tenant prefix:
```
iot/v1/+/+/+/up/#
```

---

## Time & Payload Conventions

| Rule | Convention |
|------|------------|
| Timestamps | Epoch **milliseconds**, JSON **number** |
| Booleans | JSON boolean, not `0`/`1` strings |
| Null | use JSON `null` or omit optional fields |
| Numbers | JSON number (not numeric strings) |
| Strings | UTF-8 |
| Request IDs | Decimal uint32 in topic; device/server generate independently per direction |
| Compression | Optional; negotiate out-of-band (not in v1 topic) |

---

## Comparison with Existing Platforms

| Feature | ThingsBoard | Alibaba Cloud | This Spec |
|---------|-------------|---------------|-----------|
| Topic structure | `v2/t`, `v2/a` | `/sys/{product}/{device}/...` | `iot/v1/{product}/{device}/up\|down/...` |
| Protocol version | implicit | implicit | in path |
| Presence / LWT | custom | limited | `up/status` + LWT |
| RPC | `v2/r/req/+` | `/thing/service/+/+` | `down/rpc/request/{id}` + client RPC |
| Attributes | `v2/a` | property post | reported/desired |
| Events | telemetry mix | event post | `up/events/post` |
| NTP | not built-in | `/ext/ntp/...` | `up/ntp/request/{id}` |
| OTA | not built-in | `/ota/device/...` | sha256 + sign + cancel + step codes |
| Request ID | in topic | in JSON | **in topic only** |
| Payload | flat JSON | Alink envelope | **flat JSON only** |

---

## Best Practices

1. QoS 1 for status, attributes, RPC, OTA, events; QoS 0 only where loss is acceptable.
2. Batch telemetry to cut MQTT overhead.
3. Always configure LWT + retained `up/status`.
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
- [ ] Topic builder (`v1` path, parseRequestId, capability detectors)
- [ ] LWT + retained `up/status` online/offline
- [ ] Telemetry / events / attributes / RPC builders
- [ ] Subscribe `down/#` on connect
- [ ] RPC handler registry + 30 s timeout
- [ ] NTP with request_id + numeric timestamps
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
| `v2/a/req/{id}` | `…/up/attributes/request/{id}` |
| `v2/r/req/{id}` | `…/down/rpc/request/{id}` |
| `v2/r/res/{id}` | `…/up/rpc/response/{id}` |
| `clientKeys` CSV string | `reported` / `desired` string arrays |
| RPC HTTP-like `200` | `code: 0` success |

### From Alibaba Cloud

| Old | New |
|-----|-----|
| `/sys/{product}/{device}/thing/event/property/post` | `…/up/attributes/report` |
| `/sys/.../thing/service/{method}` | `…/down/rpc/request/{id}` + `method` in JSON |
| `/ext/ntp/...` | `…/up/ntp/request/{id}` |
| `/ota/device/inform/...` | `…/up/ota/version` |
| Alink envelope | strip; use flat JSON |
| `productKey` | `product_id` |

### From previous draft of this spec (`device_type`, no version)

| Old | New |
|-----|-----|
| `iot/{device_type}/{id}/...` | `iot/v1/{product_id}/{id}/...` |
| `up/ntp/request` (no id) | `up/ntp/request/{request_id}` |
| optional envelope | removed |
| OTA `md5` | `sha256` (+ `sign` / `signMethod`) |
| attribute `clientKeys`/`sharedKeys` CSV | `reported`/`desired` arrays |
| RPC success `code: 200` | `code: 0` |

---

## Appendix: Topic Map

Device: `product_id=esp32-cam`, `device_id=cam-001`

| Direction | Topic |
|-----------|--------|
| Status | `iot/v1/esp32-cam/cam-001/up/status` |
| Telemetry | `iot/v1/esp32-cam/cam-001/up/telemetry/data` |
| Events | `iot/v1/esp32-cam/cam-001/up/events/post` |
| Attr report | `iot/v1/esp32-cam/cam-001/up/attributes/report` |
| Attr request | `iot/v1/esp32-cam/cam-001/up/attributes/request/7` |
| Attr response | `iot/v1/esp32-cam/cam-001/down/attributes/response/7` |
| Attr update | `iot/v1/esp32-cam/cam-001/down/attributes/update` |
| RPC down | `iot/v1/esp32-cam/cam-001/down/rpc/request/42` |
| RPC up | `iot/v1/esp32-cam/cam-001/up/rpc/response/42` |
| Client RPC | `iot/v1/esp32-cam/cam-001/up/rpc/request/9` |
| NTP | `iot/v1/esp32-cam/cam-001/up/ntp/request/3` → `…/down/ntp/response/3` |
| OTA version | `iot/v1/esp32-cam/cam-001/up/ota/version` |
| OTA update | `iot/v1/esp32-cam/cam-001/down/ota/update` |
| OTA cancel | `iot/v1/esp32-cam/cam-001/down/ota/cancel` |
| OTA progress | `iot/v1/esp32-cam/cam-001/up/ota/progress` |
| Logs | `iot/v1/esp32-cam/cam-001/up/logs/report` |
| Device sub | `iot/v1/esp32-cam/cam-001/down/#` |
