# Crearts IoT (device SDK)

MQTT device client for the Crearts IoT Platform (`docs/iot-platform-mqtt-spec.md`, protocol **v1**).

## Features

- `CreartsCredentials` — access token auth, LWT on status, optional TLS
- `loadOrSeedCredentials()` — NVS `fctry` (survives OTA/flash) with Kconfig seed
- `Topics` — `TopicStyle::Short` (`v1/t`, …) and `TopicStyle::Full` (`iot/v1/...`)
- Telemetry / attributes builders (`reported` flat report; `desired` request/update)
- `CreartsIotService` — subscribe downstream; telemetry, events, attributes, RPC, NTP, OTA hooks, logs
- `MetricsTelemetryBridge` — `MetricsService` → telemetry
- `CreartsOtaService` — HTTPS OTA from `v1/me/o/upd` (background task, sha256, progress; MQTT confirm / 90 s / crash-loop rollback)

## Configure (preferred)

`idf.py menuconfig` → **Embed Framework — Crearts IoT**:

| Kconfig | MQTT role |
|---------|-----------|
| `CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID` | part of username / client_id |
| `CONFIG_EMBED_CREARTS_IOT_DEVICE_ID` | part of username / client_id |
| `CONFIG_EMBED_CREARTS_IOT_HOST` | broker host (LAN IP on device) |
| `CONFIG_EMBED_CREARTS_IOT_ACCESS_TOKEN` | MQTT **password** |
| `CONFIG_EMBED_CREARTS_IOT_TOPIC_SHORT` | short vs full topics |
| `CONFIG_EMBED_CREARTS_IOT_USE_TLS` / `_PORT` | mqtts / port override |

Do **not** hardcode the access token in source. First boot copies menuconfig / local `sdkconfig` into NVS **`fctry`**; later OTA builds may leave the Kconfig token empty.

## Auth mapping

```
client_id = username = {product_id}.{device_id}
password  = <access_token>
```

RabbitMQ lab user (until the platform provisions automatically):

```bash
rabbitmqctl add_user '{product}.{device}' '<access_token>'
rabbitmqctl set_permissions -p / '{product}.{device}' '.*' '.*' '.*'
```

## Wiring

```cpp
#include "crearts_iot/crearts_iot.hpp"
#include "embed_core/nvs_store.hpp"
#include "sdkconfig.h"

embed::NvsStore::initFlash();
static auto creds = crearts::iot::loadOrSeedCredentials(
    CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID,
    CONFIG_EMBED_CREARTS_IOT_DEVICE_ID,
    CONFIG_EMBED_CREARTS_IOT_HOST,
    CONFIG_EMBED_CREARTS_IOT_ACCESS_TOKEN,
#ifdef CONFIG_EMBED_CREARTS_IOT_TOPIC_SHORT
    crearts::iot::TopicStyle::Short,
#else
    crearts::iot::TopicStyle::Full,
#endif
#ifdef CONFIG_EMBED_CREARTS_IOT_USE_TLS
    true,
#else
    false,
#endif
    static_cast<uint16_t>(CONFIG_EMBED_CREARTS_IOT_PORT));

registry.createService<embed::MqttService>(*creds);
registry.createService<embed::MetricsService>();
registry.createService<crearts::iot::CreartsIotService>(*creds);
registry.createService<crearts::iot::MetricsTelemetryBridge>();
registry.createService<crearts::iot::CreartsOtaService>();
```

Lab-only explicit user/pass: `CreartsCredentials::createBasic(...)`.

Demo `main/` also reports static **reported** attributes on connect and requests **desired** (see `CreartsDeviceInfo`).

## Spec notes

- Correlation via JSON `"id"` (not in topic)
- Attribute scopes: `reported` (device → `v1/a` / `…/attributes/report`) and `desired` (server → `v1/a/upd`)
- Dashboard: reported form is RO from device reports; desired form is edited on platform and pushed over MQTT
- RPC: register on `CreartsIotService::rpc()`; built-in **`rpc-list`** returns method + param types; unknown → `404`
- Demo handlers in `main/`: `echo`, `led_attach` / `led_detach` / `led_list`, `set_led`, `reboot`, `factory_reset`, `config_portal`, `ota_rollback`, `import_credentials`, `export_credentials`
- Presence: session = online; LWT = offline

### Register RPC methods

```cpp
using namespace crearts::iot;
static constexpr RpcParamDef kEcho[] = { rpcStr("msg") };
static constexpr RpcParamDef kSetLed[] = {
    rpcInt("gpio"), rpcInt("offset"), rpcInt("length"),
    rpcInt("r", false, 255), rpcBool("on", false, true),
};
iot->rpc().add("echo", kEcho, onEcho, nullptr, "Echo a string");
// rpc-list → params.r = { "type":"int", "required":false, "default":255 }
```

Broker stack: [deploy/README.md](../../deploy/README.md).
