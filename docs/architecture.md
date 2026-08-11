# Architecture

## Layering

```
┌─────────────────────────────────────────┐
│ main (product / demo wiring, Kconfig)   │
├─────────────────────────────────────────┤
│ crearts_iot / alicloud_* / thingsboard  │
│   (alicloud_oss: OssService + upload)   │
├─────────────────────────────────────────┤
│ embed_extra (camera, mjpeg, led strip)  │
│ embed_core  (wifi, mqtt, metrics)       │
├─────────────────────────────────────────┤
│ embed (registry, event loop, signal)    │
└─────────────────────────────────────────┘
```

Dependencies point **downward** only. Cloud providers implement `embed::MqttCredentials` and consume `MqttService` signals; they must not become required by `embed` itself.

`main/` selects a cloud path (default: **Crearts**). Credentials are loaded from NVS (`fctry`, Kconfig seed on first boot), kept `static`, and passed into `MqttService` + the vendor service.

## Crearts session model

| Concern | Mechanism |
|---------|-----------|
| Auth | MQTT `username`/`client_id` = `{product}.{device}`, `password` = access token |
| Online | MQTT session present (platform tracks CONNECT) |
| Offline | LWT on status topic (`up/status` / `v1/s`), retained |
| Reported attrs | Device → `attributes/report` / `v1/a` (flat JSON) |
| Desired attrs | Platform → `attributes/update` / `v1/a/upd`; device may request on connect |
| Correlation | JSON field `"id"` (not in topic path) |

Protocol details: [iot-platform-mqtt-spec.md](iot-platform-mqtt-spec.md).

## Event paths

| Path | Loop | Use |
|------|------|-----|
| Framework `Signal` / `Slot` | Custom `embed_evt` loop | Service-to-service POD messages |
| WiFi / IP events | Default ESP-IDF loop | Handled inside `WifiService`, then translated to Signals |
| MQTT client events | MQTT task / default loop | Handled inside `MqttService`, then translated to Signals |

Keep Slot handlers short. Blocking HTTP, TLS downloads, or long flash writes must run on a dedicated FreeRTOS task. `embed_evt` stack is `EMBED_EVENT_TASK_STACK_SIZE` (default 8192) — MQTT RPC + cJSON + LED RMT do not fit in 4 KB.

## Service size budget

Services are placement-new'd into fixed slots (`EMBED_SERVICE_SIZE`, default 512). Large state (Alink modules, buffers) should live on the heap (`std::unique_ptr`) while the registry-resident object stays small — see `AlicloudService`.

## Thread safety

With `EMBED_THREAD_SAFE=1` (default):

- `ServiceRegistry` create/get are mutex-protected
- `ConnectionPool` allocate / release / count are mutex-protected
- `EventLoop` register/unregister take the loop mutex

`Signal::emit` posts with a bounded timeout (`EMBED_EVENT_POST_TIMEOUT_MS`); on failure the message is dropped and logged.

## Ownership cheat-sheet

| Object | Owner |
|--------|-------|
| Service instances | `ServiceRegistry` pool (process lifetime) |
| MQTT credentials | Caller (`static` in `app_main`) |
| Slot connection | `Slot` → `Connection` → pool entry |
| Camera frame buffers | Camera pipeline (do not redesign without an explicit ownership plan) |
| Alink modules | `AlicloudService` via `unique_ptr` |

## Config vs source

| Lives in | Examples |
|----------|----------|
| NVS `fctry` (on device) | WiFi password, Crearts access token, broker LAN IP — survives OTA / `idf.py flash` |
| Config portal | SoftAP + HTTP (and optional STA HTTP) to write `fctry`; GPIO/RPC factory reset |
| `sdkconfig` (local, gitignored) | First-boot seed for `fctry` (skipped after factory reset / portal flag) |
| `sdkconfig.defaults` (committed) | Non-secret defaults, feature toggles |
| Source | Service graph, topic builders, protocol logic |
