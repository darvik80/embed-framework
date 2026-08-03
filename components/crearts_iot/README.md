# Crearts IoT (device SDK)

MQTT device client for the Crearts IoT Platform protocol (`docs/iot-platform-mqtt-spec.md`).

## Features

- `CreartsCredentials` — **access token** from dashboard registration (`createAccessToken`), client id `{product}.{device}`, TLS optional, LWT on status
- `Topics` — `TopicStyle::Short` (`v1/t`, …) and `TopicStyle::Full` (`iot/v1/...`)
- Telemetry / attributes builders
- `CreartsIotService` — status, telemetry, events, attributes, RPC, NTP, OTA progress hooks, logs
- `MetricsTelemetryBridge` — `MetricsService` → telemetry

## Auth

1. Register the device in the platform dashboard → copy **access token** (shown once).
2. Firmware:

```cpp
#include "crearts_iot/crearts_iot.hpp"

static auto creds = crearts::iot::CreartsCredentials::createAccessToken(
    "esp32-cam",              // product_id
    "cam-001",                // device_id
    "broker.example.com",
    "PASTE_ACCESS_TOKEN_HERE",
    crearts::iot::TopicStyle::Short);

registry.createService<embed::MqttService>(*creds);
registry.createService<embed::MetricsService>();
registry.createService<crearts::iot::CreartsIotService>(*creds);
registry.createService<crearts::iot::MetricsTelemetryBridge>();
```

MQTT mapping: `username=token`, `password=token`, `client_id=product.device`.

Lab-only explicit user/pass: `CreartsCredentials::createBasic(...)`.

## Spec notes

- Correlation via JSON `"id"` (not in topic)
- Attribute scopes: `reported` / `desired`
- RPC success code: `0`
