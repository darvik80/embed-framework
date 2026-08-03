# IoT Platform Implementation Prompt

## Context

You are implementing a standardized IoT platform integration for an ESP32 embedded framework. The platform uses MQTT (RabbitMQ MQTT plugin or any broker) and follows `docs/iot-platform-mqtt-spec.md` (**protocol `v1`**).

## Existing Codebase

Reference integrations:
- `components/thingsboard/` — ThingsBoard
- `components/alicloud_iot/` — Alibaba Cloud IoT

Shared patterns:
- Credentials provider (`embed::MqttCredentials`)
- Topic builder
- Service (`embed::Service`)
- JSON builders for telemetry / attributes / RPC
- Signal/Slot wiring

## Task

Create `components/iot_platform/` per the v1 spec.

### Requirements

1. **Credentials Provider**
   - Primary: `CreartsCredentials::createAccessToken(product, device, host, token, …)`
   - MQTT username = token, password = token, client id = `{product}.{device}`
   - LWT on status topic; optional TLS
   - Lab fallback: `createBasic(...)`

2. **Topic Builder** (`iot_platform_topics.hpp/cpp`)
   - Mirror ThingsBoard `TopicStyle` enum: `Full` | `Short`
   - **Full:** `iot/v1/{product_id}/{device_id}/{direction}/{capability}/{operation}`
   - **Short:** `v1/{code}[/{op}]` per spec mapping table (`v1/t`, `v1/a`, `v1/r/req`, …)
   - Constructor: `Topics(product_id, device_id, TopicStyle style = TopicStyle::Short)`
   - Methods (same API for both styles; no request id in path):
     - `statusPublish()` → full `up/status` / short `v1/s`
     - `telemetryPublish()` → `up/telemetry/data` / `v1/t`
     - `eventsPost()` → `up/events/post` / `v1/e`
     - `attributesReport()` → `up/attributes/report` / `v1/a`
     - `attributesRequest()` → `up/attributes/request` / `v1/a/req`
     - `attributesResponseSubscribe()` → `down/attributes/response` / `v1/a/res`
     - `attributesUpdateSubscribe()` → `down/attributes/update` / `v1/a/upd`
     - `rpcRequestSubscribe()` → `down/rpc/request` / `v1/r/req`
     - `rpcResponse()` → `up/rpc/response` / `v1/r/res`
     - `rpcClientRequest()` → `up/rpc/request` / `v1/r/creq` (optional)
     - `rpcClientResponseSubscribe()` → `down/rpc/response` / `v1/r/cres` (optional)
     - `ntpRequest()` → `up/ntp/request` / `v1/n/req`
     - `ntpResponseSubscribe()` → `down/ntp/response` / `v1/n/res`
     - `otaVersion()` → `up/ota/version` / `v1/o/ver`
     - `otaQuery()` → `up/ota/query` / `v1/o/q`
     - `otaUpdateSubscribe()` → `down/ota/update` / `v1/o/upd`
     - `otaCancelSubscribe()` → `down/ota/cancel` / `v1/o/can`
     - `otaProgress()` → `up/ota/progress` / `v1/o/p`
     - `logsReport()` → `up/logs/report` / `v1/l`
     - `downstreamSubscribe()` → full `…/down/#` / short `v1/#`
   - Helpers must accept **both** styles: `isRpcRequest`, `isAttributeUpdate`, `isAttributeResponse`, `isNtpResponse`, `isOtaUpdate`, `isOtaCancel` (correlation via JSON `id`)

3. **Telemetry Builder** — from ThingsBoard `TelemetryBuilder` (KV / ts / batch)

4. **Attribute Builder** — report flat JSON (`add` / `build` / `clear`)

5. **Attribute Request Builder**
   - Spec v1: `reported` / `desired` as **string arrays** (not CSV `clientKeys`/`sharedKeys`)
   - Support `["*"]` for all keys in a scope

6. **Service** (`iot_platform_service.hpp/cpp`)
   - Extend `embed::Service`; wire to `embed::MqttService`
   - On connect: subscribe `down/#` (or `v1/#`); presence via session + LWT only (no status publish)
   - Publish helpers: telemetry, events, attributes, RPC response, NTP, OTA version/query/progress, logs
   - Signals: `onAttributeUpdate`, `onAttributeResponse`, `onRpcRequest`, `onOtaUpdate`, `onOtaCancel`, `onNtpResponse` (as needed)
   - RPC success code is **`0`** (not HTTP 200); map unknown method → `404`
   - Correlation: put/echo numeric `"id"` in JSON body for attributes request/response, RPC, NTP

7. **Metrics Telemetry Bridge** — from ThingsBoard bridge pattern

8. **Messages** — trivially copyable `embed::Message` types; bulk JSON via fixed buffers / queues per framework rules

9. **CMakeLists.txt** / **idf_component.yml** — `embed`, `embed_core`, `json`

10. **Kconfig** (optional): `product_id`, `device_id`, broker host/port, TLS, username, password

## Component Layout

```
components/iot_platform/
├── include/iot_platform/
│   ├── iot_platform.hpp
│   ├── iot_platform_credentials.hpp
│   ├── iot_platform_topics.hpp
│   ├── iot_platform_service.hpp
│   ├── telemetry_builder.hpp
│   ├── attribute_builder.hpp
│   ├── attribute_request_builder.hpp
│   └── metrics_telemetry_bridge.hpp
├── src/
│   └── … (matching .cpp)
├── CMakeLists.txt
├── idf_component.yml
└── Kconfig.projbuild
```

## Implementation Order

1. Component skeleton (CMake, yml, umbrella header)
2. Credentials + LWT
3. Topic builder (`TopicStyle::Full` + `Short`, like ThingsBoard)
4. JSON builders (telemetry, attributes, attribute request arrays)
5. Service: connect subscribe, status, telemetry, attributes, RPC
6. Metrics bridge
7. Optional: NTP, OTA, events, logs
8. Host tests for topics + builders
9. Wire example in `main/` if requested

## Example Wiring

```cpp
#include "iot_platform/iot_platform.hpp"

static auto creds = crearts::iot::CreartsCredentials::createAccessToken(
    "esp32-cam",
    "cam-001",
    "rabbitmq.local",
    "DEVICE_ACCESS_TOKEN",
    crearts::iot::TopicStyle::Short);

registry.createService<embed::MqttService>(*creds);
registry.createService<embed::MetricsService>();
registry.createService<crearts::iot::CreartsIotService>(*creds);
registry.createService<crearts::iot::MetricsTelemetryBridge>();
```

## Testing Checklist

- [ ] Client ID `{product}.{device}`
- [ ] Topics match full `iot/v1/...` **and** short `v1/...` for the same API
- [ ] LWT configured on status topic (no app online/offline publish)
- [ ] Downstream subscribe: `…/down/#` or `v1/#`
- [ ] Attribute request JSON uses arrays + `"id"`
- [ ] RPC response `code: 0` and echoes `"id"`
- [ ] Correlation via body `id` (not topic)
- [ ] Metrics bridge publishes telemetry
- [ ] NTP request/response include body `id`

## Code Style

Follow embed-framework conventions (`ServiceRegistry`, Signal/Slot, trivially-copyable Messages, `embed::string<N>`, cJSON, `ESP_LOGx`). See `.cursor/skills/embed-framework/SKILL.md` and `embed-new-component` / `embed-new-service` skills.

## Migration (topics)

| From | To |
|------|-----|
| ThingsBoard `v2/t` | `iot/v1/{product}/{id}/up/telemetry/data` |
| ThingsBoard `v2/a` | `…/up/attributes/report` |
| ThingsBoard `v2/r/req/+` | `…/down/rpc/request` + `"id"` in JSON |
| Alicloud property post | `…/up/attributes/report` |
| Alicloud service invoke | `…/down/rpc/request` + `method` / `id` in JSON |
| Draft spec `iot/{type}/{id}/…` | `iot/v1/{product_id}/{id}/…` |
| Id in topic (`…/request/42`) | stable topic + `"id": 42` in body |
| ThingsBoard short `v2/t` | our short `v1/t` (and full form still supported) |

## Success Criteria

- Matches `docs/iot-platform-mqtt-spec.md` v1
- Compiles; topic/builder tests pass
- Core path: status, telemetry, attributes, RPC
- NTP / OTA / events / logs can land in follow-up PRs

## Notes

- Flat JSON only — no Alink envelope
- Correlation only via JSON field `id` (not in topic)
- Support both `TopicStyle::Full` and `TopicStyle::Short`; default Short on device
- Do not mix styles on one connection
- Start simple; do not port every Alicloud module on day one
- Read the spec before inventing alternate topic shapes
