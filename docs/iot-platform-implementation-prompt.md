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

1. **Credentials Provider** (`iot_platform_credentials.hpp/cpp`)
   - Implement `embed::MqttCredentials`
   - Username/password; optional TLS
   - Client ID: `{product_id}.{device_id}`
   - Configure LWT: topic `…/up/status`, retained, QoS 1, offline JSON (`online:false`, `reason:"lwt"`)
   - Store broker URI, username, password, `product_id`, `device_id`

2. **Topic Builder** (`iot_platform_topics.hpp/cpp`)
   - Pattern: `iot/v1/{product_id}/{device_id}/{direction}/{capability}/{operation}[/{request_id}]`
   - Methods:
     - `statusPublish()` → `up/status`
     - `telemetryPublish()` → `up/telemetry/data`
     - `eventsPost()` → `up/events/post`
     - `attributesReport()` → `up/attributes/report`
     - `attributesRequest(uint32_t id)` → `up/attributes/request/{id}`
     - `attributesResponseSubscribe()` → `down/attributes/response/+`
     - `attributesUpdateSubscribe()` → `down/attributes/update`
     - `rpcRequestSubscribe()` → `down/rpc/request/+`
     - `rpcResponse(uint32_t id)` → `up/rpc/response/{id}`
     - `rpcClientRequest(uint32_t id)` → `up/rpc/request/{id}` (optional)
     - `rpcClientResponseSubscribe()` → `down/rpc/response/+` (optional)
     - `ntpRequest(uint32_t id)` → `up/ntp/request/{id}`
     - `ntpResponseSubscribe()` → `down/ntp/response/+`
     - `otaVersion()` → `up/ota/version`
     - `otaQuery()` → `up/ota/query`
     - `otaUpdateSubscribe()` → `down/ota/update`
     - `otaCancelSubscribe()` → `down/ota/cancel`
     - `otaProgress()` → `up/ota/progress`
     - `logsReport()` → `up/logs/report`
     - `downstreamSubscribe()` → `down/#`
   - Helpers: `parseRequestId`, `isRpcRequest`, `isAttributeUpdate`, `isAttributeResponse`, `isNtpResponse`, `isOtaUpdate`, `isOtaCancel`

3. **Telemetry Builder** — from ThingsBoard `TelemetryBuilder` (KV / ts / batch)

4. **Attribute Builder** — report flat JSON (`add` / `build` / `clear`)

5. **Attribute Request Builder**
   - Spec v1: `reported` / `desired` as **string arrays** (not CSV `clientKeys`/`sharedKeys`)
   - Support `["*"]` for all keys in a scope

6. **Service** (`iot_platform_service.hpp/cpp`)
   - Extend `embed::Service`; wire to `embed::MqttService`
   - On connect: subscribe `down/#`; publish retained online `up/status`
   - Publish helpers: telemetry, events, attributes, RPC response, NTP, OTA version/query/progress, logs
   - Signals: `onAttributeUpdate`, `onAttributeResponse`, `onRpcRequest`, `onOtaUpdate`, `onOtaCancel`, `onNtpResponse` (as needed)
   - RPC success code is **`0`** (not HTTP 200); map unknown method → `404`

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
3. Topic builder (`iot/v1/...`)
4. JSON builders (telemetry, attributes, attribute request arrays)
5. Service: connect subscribe, status, telemetry, attributes, RPC
6. Metrics bridge
7. Optional: NTP, OTA, events, logs
8. Host tests for topics + builders
9. Wire example in `main/` if requested

## Example Wiring

```cpp
#include "iot_platform/iot_platform.hpp"

static iot_platform::IotPlatformCredentials creds(
    "esp32-cam",          // product_id
    "cam-001",            // device_id
    "mqtt://rabbitmq:1883",
    "user",
    "pass"
);

registry.createService<embed::MqttService>(creds);
registry.createService<embed::MetricsService>();
registry.createService<iot_platform::IotPlatformService>();
registry.createService<iot_platform::MetricsTelemetryBridge>();
```

## Testing Checklist

- [ ] Client ID `{product}.{device}`
- [ ] Topics match `iot/v1/{product}/{device}/...`
- [ ] LWT + online status retained publish
- [ ] `down/#` subscribed on connect
- [ ] Attribute request JSON uses arrays
- [ ] RPC response `code: 0` on success
- [ ] Request id parsed from topic
- [ ] Metrics bridge publishes telemetry
- [ ] NTP request includes request_id segment

## Code Style

Follow embed-framework conventions (`ServiceRegistry`, Signal/Slot, trivially-copyable Messages, `embed::string<N>`, cJSON, `ESP_LOGx`). See `.cursor/skills/embed-framework/SKILL.md` and `embed-new-component` / `embed-new-service` skills.

## Migration (topics)

| From | To |
|------|-----|
| ThingsBoard `v2/t` | `iot/v1/{product}/{id}/up/telemetry/data` |
| ThingsBoard `v2/a` | `…/up/attributes/report` |
| ThingsBoard `v2/r/req/+` | `…/down/rpc/request/+` |
| Alicloud property post | `…/up/attributes/report` |
| Alicloud service invoke | `…/down/rpc/request/{id}` |
| Draft spec `iot/{type}/{id}/…` | `iot/v1/{product_id}/{id}/…` |

## Success Criteria

- Matches `docs/iot-platform-mqtt-spec.md` v1
- Compiles; topic/builder tests pass
- Core path: status, telemetry, attributes, RPC
- NTP / OTA / events / logs can land in follow-up PRs

## Notes

- Flat JSON only — no Alink envelope
- Correlation only via topic `request_id`
- Start simple; do not port every Alicloud module on day one
- Read the spec before inventing alternate topic shapes
