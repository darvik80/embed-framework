# Architecture

## Layering

```
┌─────────────────────────────────────────┐
│ main (demo / product wiring)            │
├─────────────────────────────────────────┤
│ alicloud_iot / alicloud_oss / vendors   │
├─────────────────────────────────────────┤
│ embed_extra (camera, mjpeg, oss upload) │
│ embed_core  (wifi, mqtt, metrics)       │
├─────────────────────────────────────────┤
│ embed (registry, event loop, signal)    │
└─────────────────────────────────────────┘
```

Dependencies point **downward** only. Cloud providers implement `embed::MqttCredentials` and consume `MqttService` signals; they must not become required by `embed` itself.

## Event paths

| Path | Loop | Use |
|------|------|-----|
| Framework `Signal` / `Slot` | Custom `embed_evt` loop | Service-to-service POD messages |
| WiFi / IP events | Default ESP-IDF loop | Handled inside `WifiService`, then translated to Signals |
| MQTT client events | MQTT task / default loop | Handled inside `MqttService`, then translated to Signals |

Keep Slot handlers short. Blocking HTTP, TLS downloads, or long flash writes must run on a dedicated FreeRTOS task.

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
