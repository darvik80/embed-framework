# thingsboard

ThingsBoard MQTT device API for embed-framework.

- [Getting Connected](https://thingsboard.io/docs/reference/mqtt-api/getting-connected/)
- [Telemetry](https://thingsboard.io/docs/reference/mqtt-api/telemetry/)
- [Attributes](https://thingsboard.io/docs/reference/mqtt-api/attributes/)
- [RPC](https://thingsboard.io/docs/reference/mqtt-api/rpc/)

## Features

| Piece | Role |
|-------|------|
| `ThingsBoardCredentials` | Access Token + MQTT Basic |
| `Topics` | Short / Standard topic helpers |
| `TelemetryBuilder` / `TelemetryBatch` | Telemetry JSON |
| `AttributeBuilder` / `AttributeRequestBuilder` | Client attrs + get-request |
| `parseAttributeResponse` / `parseAttributeUpdate` | Parse replies / shared pushes |
| `RpcBuilder` / `RpcReplyBuilder` | Client RPC requests + two-way replies |
| `ThingsBoardService` | Publish / request / RPC + Signals |
| `MetricsTelemetryBridge` | Metrics → telemetry |

## RPC

### Server-side (platform → device)

Subscribe is automatic (`v2/r/req/+`). Handle `onRpcRequest` and reply with `respondRpc`:

```cpp
tb->onRpcRequest /* Slot */ ;
// msg.method / msg.params / msg.requestId

thingsboard::RpcReplyBuilder reply;
reply.add("ok", true).add("value", 26);
tb->respondRpc(msg.requestId, reply);
// → v2/r/res/$request_id
```

### Client-side (device → platform)

```cpp
thingsboard::RpcBuilder req;
req.method("getCurrentTime").paramsEmptyObject();
uint32_t id = 0;
tb->requestRpc(req, id);
// → v2/r/req/$id ; reply on onRpcResponse (v2/r/res/+)
```

### Session limits

Built-in client-side RPC for rate-limit self-throttling:

```cpp
uint32_t id = 0;
tb->requestSessionLimits(id);
// {"method":"getSessionLimits","params":{}}
```

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

- Device claim API
- Protobuf payloads
