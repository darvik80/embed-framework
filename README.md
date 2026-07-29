# embed-framework

C++20 service framework for **ESP-IDF ≥ 5.5** (ESP32-S3). Provides a fixed-size `ServiceRegistry`, Signal/Slot messaging on a dedicated `esp_event` loop, CRTP state machines, and layered components for WiFi/MQTT, camera, and Alibaba Cloud.

## Quick start

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

Configure WiFi / MQTT / Alicloud secrets via `idf.py menuconfig` (menus under **Embed Framework**) or `sdkconfig.defaults`.

## Architecture

```
main/                     app wiring (create services, credentials)
components/embed/         framework: Service, Registry, EventLoop, Signal/Slot, StateMachine
components/embed_core/    WifiService, MqttService, MetricsService
components/embed_extra/   CameraService, MjpegService, OssUploadService
components/alicloud_*     Alibaba IoT / OSS
components/thingsboard/   stub (incomplete)
```

### Lifecycle

1. `embed::EventLoop::instance().init()`
2. Create credentials that **outlive** MQTT (`static` in `app_main`)
3. `registry.createService<T>(...)` for each service
4. `registry.startAll()` — peers exist; connect `Slot`s inside `start()`
5. Idle the main task; work runs on the embed event task / FreeRTOS tasks

### Messages

Types posted through `Signal` must satisfy `embed::Message`:

- trivially copyable + standard layout
- `sizeof(T) ≤ EMBED_MAX_EVENT_DATA_SIZE` (default **1024**)

Prefer POD and `embed::string<N>`. Do **not** put owning buffers (camera frames, heap pointers) in multi-subscriber Signals — use a FreeRTOS queue with a clear reclaim contract.

### Config knobs (`embed/config.hpp`)

| Macro | Default | Meaning |
|-------|---------|---------|
| `EMBED_MAX_SERVICES` | 16 | Registry slots |
| `EMBED_SERVICE_SIZE` | 512 | Max bytes per service object |
| `EMBED_MAX_CONNECTIONS` | 64 | Signal/Slot connection pool |
| `EMBED_EVENT_QUEUE_SIZE` | 32 | Embed event queue depth |
| `EMBED_EVENT_POST_TIMEOUT_MS` | 100 | Post wait; drop + log on timeout (`-1` = forever) |
| `EMBED_MAX_EVENT_DATA_SIZE` | 1024 | Max `Message` size |
| `EMBED_THREAD_SAFE` | 1 | Mutexes on registry / connection pool |

## Components

| Component | Role |
|-----------|------|
| [embed](components/embed/README.md) | Core framework |
| [embed_core](components/embed_core/README.md) | WiFi, MQTT, metrics |
| embed_extra | Camera / MJPEG / OSS upload |
| alicloud_iot | Alink modules (things, OTA, NTP, …) |
| alicloud_oss | OSS client + `OssService` |

## MQTT reconnect

`MqttService` owns reconnect: state machine + `esp_timer` using `CONFIG_EMBED_MQTT_MAX_RETRY` and `CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS`. esp-mqtt **auto-reconnect is disabled** so the two policies do not fight.

Incoming payloads larger than `MqttMessageReceived::payload` capacity (767) are truncated with a warning log.

## OTA

Factory-only default table cannot OTA. Use `partitions_ota.csv` and a background task — see [docs/ota.md](docs/ota.md).

## Metrics storage

With `CONFIG_EMBED_METRICS_ENABLE_STORAGE`, metrics use the `storage` **SPIFFS** partition (`partitions.csv`). `storageUsedBytes` comes from `esp_spiffs_info` when mounted; otherwise total size is reported and used stays 0.

## Tests (Unity)

See [docs/testing.md](docs/testing.md). Quick path:

```bash
cd test_apps/embed_unity
idf.py set-target esp32s3
idf.py build flash monitor
```

## CI (Gitea)

See [docs/ci.md](docs/ci.md). Workflows: `.gitea/workflows/ci.yml` (mirrored under `.github/workflows/`).

## Further reading

- [Architecture notes](docs/architecture.md)
- [OTA](docs/ota.md)
- [CI](docs/ci.md)
- [TODO](TODO.md)
- Agent skills: `.cursor/skills/embed-framework`, `embed-new-service`, `embed-new-component`
