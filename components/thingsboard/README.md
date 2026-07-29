# thingsboard

ThingsBoard MQTT device API for embed-framework.

- [Getting Connected](https://thingsboard.io/docs/reference/mqtt-api/getting-connected/)
- [Telemetry](https://thingsboard.io/docs/reference/mqtt-api/telemetry/)
- [Attributes](https://thingsboard.io/docs/reference/mqtt-api/attributes/)

## Features

| Piece | Role |
|-------|------|
| `ThingsBoardCredentials` | Access Token + MQTT Basic |
| `Topics` | Short / Standard topic helpers |
| `TelemetryBuilder` / `TelemetryBatch` | Telemetry JSON |
| `AttributeBuilder` / `AttributeRequestBuilder` | Client attrs + get-request |
| `parseAttributeResponse` / `parseAttributeUpdate` | Parse replies / shared pushes |
| `ThingsBoardService` | Publish / request / RPC + Signals |
| `MetricsTelemetryBridge` | Metrics → telemetry |

## Attributes

### Publish client-side (`v2/a`)

```cpp
thingsboard::AttributeBuilder attrs;
attrs.add("firmwareVersion", "2.1.0")
     .add("serialNumber", "SN-4A21F");
tb->publishAttributes(attrs);
```

### Request values (`v2/a/req/$id` → `v2/a/res/+`)

```cpp
thingsboard::AttributeRequestBuilder req;
req.clientKeys("firmwareVersion,serialNumber")
   .sharedKeys("targetTemperature,enabled");
uint32_t reqId = 0;
tb->requestAttributes(req, reqId);

// onAttributeResponse:
auto v = thingsboard::parseAttributeResponse(msg.payload.c_str());
double sp = 0;
thingsboard::attributeGetNumber(v.sharedJson, "targetTemperature", sp);
```

### Shared updates (push on `v2/a`)

```cpp
// onAttributeUpdate:
auto v = thingsboard::parseAttributeUpdate(msg.payload.c_str());
// v.sharedJson == {"targetTemperature":26}
```

## Telemetry

```cpp
thingsboard::TelemetryBuilder b;
b.add("temperature", 22.5).add("humidity", 61);
tb->publishTelemetry(b);

b.timestampMs(1451649600512LL); // → {"ts":...,"values":{...}}
```

## Host tests

```bash
cmake -S host_test -B host_test/build && cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

## Next

- Client-side RPC / claim
- Protobuf payloads
