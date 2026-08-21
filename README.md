# embed-framework

C++20 service framework for **ESP-IDF ≥ 5.5** (ESP32-S3): fixed-size `ServiceRegistry`, Signal/Slot on a dedicated `esp_event` loop, CRTP state machines, and cloud device SDKs (Cogitor IoT, Alibaba Cloud, ThingsBoard).

**[Developer Manual](docs/embed-framework-manual.md)** — comprehensive guide covering all components, API reference, code examples, and iot-platform-go integration.

## Quick start

```bash
idf.py set-target esp32s3
idf.py menuconfig   # Embed Framework → WiFi + Cogitor IoT
idf.py build flash monitor
```

**PSRAM:** `sdkconfig.defaults` assumes **octal** PSRAM (ESP32-S3-WROOM-1-**N8R8** / Freenove CAM). `quad_psram: chip is not connected, or wrong PSRAM line mode` + `Failed to init external RAM!` means the image was built for **quad** (or the module has no PSRAM). That abort is in `cpu_start` — **before** `app_main`, so firmware crash-loop rollback never runs. Fix: Component config → ESP PSRAM → **Octal** for R8, **Quad** for R2, or disable SPIRAM if there is no `R` in the module name. `CONFIG_SPIRAM_IGNORE_NOTFOUND` keeps the chip booting if the mode is wrong.

**Secrets and site config:** first boot seeds NVS partition **`fctry`** from local `sdkconfig` (gitignored). After that OTA / USB flash keep WiFi + Cogitor token on device. Commit only `sdkconfig.defaults` (no access tokens).

| Menuconfig path | Keys |
|-----------------|------|
| Embed Framework — WiFi | `CONFIG_EMBED_WIFI_*` |
| Embed Framework — Cogitor IoT | `CONFIG_EMBED_COGITOR_IOT_*` (product, device, host, **token**, TLS, topics) |
| Embed Framework — MQTT / Metrics | `CONFIG_EMBED_MQTT_*`, `CONFIG_EMBED_METRICS_*` |

Demo `main/` wires **Cogitor** by default: WiFi → MQTT → `CogitorIotService` (+ metrics bridge, `CogitorDeviceInfo` attributes, RPC demo).

## Architecture

```
main/                      app wiring (services, Kconfig → NVS seed → credentials)
components/embed/          Service, Registry, EventLoop, Signal/Slot, StateMachine
components/embed_core/     WifiService, MqttService, MetricsService, NvsStore
components/embed_extra/    Camera, MJPEG, WS2812 LED strip
components/cogitor_iot/    Cogitor IoT Platform device SDK (protocol v1)
components/alicloud_*      Alibaba IoT / OSS (+ camera frame upload)
components/thingsboard/    ThingsBoard MQTT device API
deploy/                    RabbitMQ + Node-RED lab stack
```

### Lifecycle

1. `embed::EventLoop::instance().init()`
2. `NvsStore::initFlash()` then build credentials that **outlive** MQTT (`static` in `app_main`, NVS with Kconfig seed)
3. `registry.createService<T>(...)` for each service
4. `registry.startAll()` — peers exist; connect `Slot`s inside `start()`
5. Idle the main task; work runs on the embed event task / FreeRTOS tasks

### Messages

Types posted through `Signal` must satisfy `embed::Message`:

- trivially copyable + standard layout
- `sizeof(T) ≤ EMBED_MAX_EVENT_DATA_SIZE` (default **1600**)

Prefer POD and `embed::string<N>`. Do **not** put owning buffers (camera frames, heap pointers) in multi-subscriber Signals — use a FreeRTOS queue with a clear reclaim contract.

### Config knobs (`embed/config.hpp`)

| Macro | Default | Meaning |
|-------|---------|---------|
| `EMBED_MAX_SERVICES` | 16 | Registry slots |
| `EMBED_SERVICE_SIZE` | 512 | Max bytes per service object |
| `EMBED_MAX_CONNECTIONS` | 64 | Signal/Slot connection pool |
| `EMBED_EVENT_TASK_STACK_SIZE` | 8192 | `embed_evt` stack (RPC + cJSON + LED RMT) |
| `EMBED_EVENT_QUEUE_SIZE` | 32 | Embed event queue depth |
| `EMBED_EVENT_POST_TIMEOUT_MS` | 100 | Post wait; drop + log on timeout (`-1` = forever) |
| `EMBED_MAX_EVENT_DATA_SIZE` | 1600 | Max `Message` size |
| `EMBED_THREAD_SAFE` | 1 | Mutexes on registry / connection pool |

## Cogitor IoT (default demo path)

Protocol: [docs/iot-platform-mqtt-spec.md](docs/iot-platform-mqtt-spec.md) (`v1`).

Device MQTT auth (RabbitMQ):

```
client_id = username = {product_id}.{device_id}
password  = <access_token>
```

Presence: **online** = MQTT session; **offline** = LWT on status (`up/status` / `v1/s`).  
Attributes: **reported** from device on connect; **desired** from dashboard via `attributes/update`.

```cpp
embed::NvsStore::initFlash();
static auto creds = cogitor::iot::loadOrSeedCredentials(
    CONFIG_EMBED_COGITOR_IOT_PRODUCT_ID,
    CONFIG_EMBED_COGITOR_IOT_DEVICE_ID,
    CONFIG_EMBED_COGITOR_IOT_HOST,
    CONFIG_EMBED_COGITOR_IOT_ACCESS_TOKEN,
    /* TopicStyle / TLS / port from Kconfig — used only if NVS empty */);
```

Lab broker + Node-RED: [deploy/README.md](deploy/README.md).  
SDK details: [components/cogitor_iot/README.md](components/cogitor_iot/README.md) · [Developer Manual §7](docs/embed-framework-manual.md#7-cogitor_iot--cogitor-iot-platform-sdk).

Until the platform provisions broker users, create the MQTT user manually:

```bash
docker exec cogitor-rabbitmq rabbitmqctl add_user 'home.esp32-s3' '<access_token>'
docker exec cogitor-rabbitmq rabbitmqctl set_permissions -p / 'home.esp32-s3' '.*' '.*' '.*'
```

Point the device at the **Go platform** MQTT broker (**LAN IP**, not `localhost`). Do not use `fix-podman-ports.ps1` unless you are still running the legacy Podman RabbitMQ stack.

## Components

| Component | Role |
|-----------|------|
| [embed](components/embed/README.md) | Core framework |
| [embed_core](components/embed_core/README.md) | WiFi, MQTT, metrics |
| embed_extra | Camera / MJPEG / WS2812 LED strip |
| [cogitor_iot](components/cogitor_iot/README.md) | Cogitor IoT Platform device SDK |
| alicloud_iot | Alink modules (things, OTA, NTP, …) |
| alicloud_oss | OSS client + `OssService` + `OssUploadService` |
| [thingsboard](components/thingsboard/README.md) | ThingsBoard MQTT device API |

Full API reference and usage examples: **[Developer Manual](docs/embed-framework-manual.md)**.

## MQTT reconnect

`MqttService` owns reconnect: state machine + `esp_timer` using `CONFIG_EMBED_MQTT_MAX_RETRY` and `CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS`. esp-mqtt **auto-reconnect is disabled** so the two policies do not fight.

Incoming payloads larger than `MqttMessageReceived::payload` capacity (1400) are truncated with a warning log.

## OTA

Factory-only default table cannot OTA. Use `partitions_ota.csv` and `CogitorOtaService` — see [docs/ota.md](docs/ota.md). Device identity is in **`fctry`** NVS (survives OTA and `idf.py flash`).

## Config portal / factory reset

WiFi + Cogitor token live in `fctry`. To **force-update** them:

| Trigger | Effect |
|---------|--------|
| Hold **BOOT** (GPIO 0) 3 s **while running** | Wipe `fctry`, reboot into SoftAP `embed-XXXX` |
| Press **EN/RST** 3× quickly (<10 s) | Same wipe + portal (RST is not a GPIO) |
| RPC `factory_reset` `{ "confirm": true }` | Same wipe + reboot into AP |
| RPC `config_portal` | Reboot into AP **without** wipe (form prefilled) |
| Web **Factory reset credentials** | Wipe active creds (backup kept) + reboot into AP |
| Web **Save & reboot** | Snapshot backup, write `fctry`, STA + MQTT |
| Web **Import JSON** / `GET /credentials.json` | Paste or upload credentials file; download current |
| RPC `import_credentials` `{ "json": "{…}" }` | Same import + reboot |
| Web **Restore backup** | Swap active ↔ previous **settings**, reboot |
| Web / RPC **ota_rollback** | Boot previous **firmware** OTA slot |
| OTA crash-loop before init | Bootloader pending-verify + 3 failed boots → previous slot |

On STA (default), the same page is at `http://<device-ip>/`. Menuconfig: **Embed Framework — Config Portal**.

Credentials JSON (web import / RPC `import_credentials` / download):

```json
{
  "wifi": { "ssid": "home", "password": "secret" },
  "cogitor": {
    "product": "home",
    "device": "esp32-s3",
    "host": "192.168.1.100",
    "port": 0,
    "token": "…",
    "tls": false,
    "topic_short": true
  }
}
```

## Metrics storage

With `CONFIG_EMBED_METRICS_ENABLE_STORAGE`, metrics use the `storage` **SPIFFS** partition (`partitions.csv`). `storageUsedBytes` comes from `esp_spiffs_info` when mounted; otherwise total size is reported and used stays 0.

## Tests (Unity)

**Host (no flash)** — CI gate:

```bash
cmake -S host_test -B host_test/build
cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

See [host_test/README.md](host_test/README.md) and [docs/testing.md](docs/testing.md).

Device app (optional): `test_apps/embed_unity`.

## CI (Gitea)

See [docs/ci.md](docs/ci.md). Workflows: `.gitea/workflows/ci.yml` (mirrored under `.github/workflows/`).

## Documentation map

| Doc | Contents |
|-----|----------|
| [**Developer Manual**](docs/embed-framework-manual.md) | Full guide: all components, API, examples, iot-platform-go integration |
| [docs/architecture.md](docs/architecture.md) | Layering, events, ownership |
| [docs/iot-platform-mqtt-spec.md](docs/iot-platform-mqtt-spec.md) | Cogitor MQTT protocol v1 |
| [docs/iot-platform-service-prompt.md](docs/iot-platform-service-prompt.md) | Platform (Go/React/Node-RED) design prompt |
| [docs/iot-platform-implementation-prompt.md](docs/iot-platform-implementation-prompt.md) | Device SDK notes (implemented as `cogitor_iot`) |
| [deploy/README.md](deploy/README.md) | RabbitMQ + Node-RED lab; Podman/Synology tips |
| [docs/ota.md](docs/ota.md) | OTA partitions |
| [docs/testing.md](docs/testing.md) / [docs/ci.md](docs/ci.md) | Tests & CI |
| [TODO.md](TODO.md) | Backlog |

Agent skills: `.cursor/skills/embed-framework`, `embed-new-service`, `embed-new-component`.
