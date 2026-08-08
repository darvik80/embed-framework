# Crearts IoT (device SDK)

MQTT device client for the Crearts IoT Platform (`docs/iot-platform-mqtt-spec.md`, protocol **v1**).

## Features

- `CreartsCredentials` — access token auth, LWT on status, optional TLS
- `Topics` — `TopicStyle::Short` (`v1/t`, …) and `TopicStyle::Full` (`iot/v1/...`)
- Telemetry / attributes builders (`reported` flat report; `desired` request/update)
- `CreartsIotService` — subscribe downstream; telemetry, events, attributes, RPC, NTP, OTA hooks, logs
- `MetricsTelemetryBridge` — `MetricsService` → telemetry

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

Do **not** hardcode the access token in source. Keep it in local `sdkconfig` only.

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
#include "sdkconfig.h"

static auto creds = crearts::iot::CreartsCredentials::createAccessToken(
    CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID,
    CONFIG_EMBED_CREARTS_IOT_DEVICE_ID,
    CONFIG_EMBED_CREARTS_IOT_HOST,
    CONFIG_EMBED_CREARTS_IOT_ACCESS_TOKEN,
#ifdef CONFIG_EMBED_CREARTS_IOT_TOPIC_SHORT
    crearts::iot::TopicStyle::Short
#else
    crearts::iot::TopicStyle::Full
#endif
);

registry.createService<embed::MqttService>(*creds);
registry.createService<embed::MetricsService>();
registry.createService<crearts::iot::CreartsIotService>(*creds);
registry.createService<crearts::iot::MetricsTelemetryBridge>();
```

Lab-only explicit user/pass: `CreartsCredentials::createBasic(...)`.

Demo `main/` also reports static **reported** attributes on connect and requests **desired** (see `CreartsDeviceInfo`).

## Spec notes

- Correlation via JSON `"id"` (not in topic)
- Attribute scopes: `reported` (device → `v1/a` / `…/attributes/report`) and `desired` (server → `v1/a/upd`)
- Dashboard: reported form is RO from device reports; desired form is edited on platform and pushed over MQTT
- RPC: `RpcParams` / `parseRpcRequest`; success code `0`; unknown method `404`
- Demo handlers in `main/`: `echo`, `set_led` (`offset`/`length`), `reboot`
- Presence: session = online; LWT = offline

Broker stack: [deploy/README.md](../../deploy/README.md).
