# IoT Platform Implementation Prompt

## Status

Device SDK is implemented as **`components/cogitor_iot/`** (not a separate `iot_platform/` tree).
Use this document when extending the SDK or aligning a new product app with protocol **v1**.

Canonical references:
- Spec: `docs/iot-platform-mqtt-spec.md`
- SDK README: `components/cogitor_iot/README.md`
- Platform (Go/React) prompt: `docs/iot-platform-service-prompt.md`
- Lab broker: `deploy/README.md`

## Context

You are implementing or extending a standardized IoT platform integration for an ESP32 embedded framework. The platform uses MQTT (RabbitMQ MQTT plugin or any broker) and follows `docs/iot-platform-mqtt-spec.md` (**protocol `v1`**).

## Existing Codebase

Reference integrations:
- `components/cogitor_iot/` — **Cogitor (current / preferred)**
- `components/thingsboard/` — ThingsBoard
- `components/alicloud_iot/` — Alibaba Cloud IoT

Shared patterns:
- Credentials provider (`embed::MqttCredentials`)
- Topic builder
- Service (`embed::Service`)
- JSON builders for telemetry / attributes / RPC
- Signal/Slot wiring
- Kconfig (`CONFIG_EMBED_COGITOR_IOT_*`) for host / ids / token — never hardcode tokens in source

## Task (historical / extension)

Originally: create a device SDK per the v1 spec. **Done** as `cogitor_iot`.
When adding features, keep API and topics aligned with the spec and the checklist below.

### Requirements (contract)

1. **Credentials Provider**
   - Primary: `CogitorCredentials::createAccessToken(product, device, host, token, …)`
   - MQTT username = `{product}.{device}`, password = token, client id = `{product}.{device}`
   - LWT on status topic; optional TLS
   - Lab fallback: `createBasic(...)`
   - App wiring: values from `CONFIG_EMBED_COGITOR_IOT_*` (menuconfig / local `sdkconfig`)

2. **Topic Builder** (`topics.hpp` / `topics.cpp` under `cogitor_iot`)
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

3. **Telemetry Builder** — KV / ts / batch

4. **Attribute Builder** — report flat JSON (`add` / `build` / `clear`)

5. **Attribute Request Builder**
   - Spec v1: `reported` / `desired` as **string arrays** (not CSV `clientKeys`/`sharedKeys`)
   - Support `["*"]` for all keys in a scope

6. **Service** (`CogitorIotService`)
   - Extend `embed::Service`; wire to `embed::MqttService`
   - On connect: subscribe `down/#` (or `v1/#`); presence via session + LWT only (no status publish)
   - Publish helpers: telemetry, events, attributes, RPC response, NTP, OTA version/query/progress, logs
   - Signals: `onAttributeUpdate`, `onAttributeResponse`, `onRpcRequest`, `onOtaUpdate`, `onOtaCancel`, `onNtpResponse` (as needed)
   - RPC success code is **`0`** (not HTTP 200); map unknown method → `404`
   - Correlation: put/echo numeric `"id"` in JSON body for attributes request/response, RPC, NTP

7. **Metrics Telemetry Bridge**

8. **Messages** — trivially copyable `embed::Message` types; bulk JSON via fixed buffers / queues per framework rules

9. **CMakeLists.txt** / **idf_component.yml** — `embed`, `embed_core`, `json`

10. **Kconfig** — `CONFIG_EMBED_COGITOR_IOT_*` (product, device, host, token, TLS, port, topic style)

## Component Layout (current)

```
components/cogitor_iot/
├── include/cogitor_iot/
│   ├── cogitor_iot.hpp
│   ├── credentials.hpp
│   ├── topics.hpp
│   ├── topic_strings.hpp
│   ├── cogitor_iot_service.hpp
│   ├── telemetry.hpp
│   ├── attributes.hpp
│   └── metrics_telemetry_bridge.hpp
├── src/
│   └── … (matching .cpp)
├── CMakeLists.txt
├── idf_component.yml
├── Kconfig.projbuild
└── README.md
```

## Extension order

1. Keep topics + builders aligned with spec changes
2. Service helpers / signals as needed
3. Host tests for topics + builders
4. Demo wiring stays in `main/` (Kconfig-driven)

## Example Wiring

```cpp
#include "cogitor_iot/cogitor_iot.hpp"
#include "sdkconfig.h"

static auto creds = cogitor::iot::CogitorCredentials::createAccessToken(
    CONFIG_EMBED_COGITOR_IOT_PRODUCT_ID,
    CONFIG_EMBED_COGITOR_IOT_DEVICE_ID,
    CONFIG_EMBED_COGITOR_IOT_HOST,
    CONFIG_EMBED_COGITOR_IOT_ACCESS_TOKEN,
#ifdef CONFIG_EMBED_COGITOR_IOT_TOPIC_SHORT
    cogitor::iot::TopicStyle::Short
#else
    cogitor::iot::TopicStyle::Full
#endif
);

registry.createService<embed::MqttService>(*creds);
registry.createService<embed::MetricsService>();
registry.createService<cogitor::iot::CogitorIotService>(*creds);
registry.createService<cogitor::iot::MetricsTelemetryBridge>();
```

## Testing Checklist

- [ ] Client ID `{product}.{device}`; username same; password = token
- [ ] Topics match full `iot/v1/...` **and** short `v1/...` for the same API
- [ ] LWT configured on status topic (no app online/offline publish)
- [ ] Downstream subscribe: `…/down/#` or `v1/#`
- [ ] Attribute request JSON uses arrays + `"id"`
- [ ] RPC response `code: 0` and echoes `"id"`
- [ ] Correlation via body `id` (not topic)
- [ ] Metrics bridge publishes telemetry
- [ ] NTP request/response include body `id`
- [ ] Credentials come from `CONFIG_EMBED_COGITOR_IOT_*` (not source literals)

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
