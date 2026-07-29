# thingsboard

ThingsBoard MQTT device API for embed-framework.

- [Getting Connected](https://thingsboard.io/docs/reference/mqtt-api/getting-connected/)
- [Telemetry](https://thingsboard.io/docs/reference/mqtt-api/telemetry/)

## Features

| Piece | Role |
|-------|------|
| `ThingsBoardCredentials` | Access Token + MQTT Basic → `embed::MqttCredentials` |
| `Topics` | Short (`v2/...`) / Standard (`v1/devices/me/...`) |
| `TelemetryBuilder` / `TelemetryBatch` | JSON payloads per Telemetry API |
| `ThingsBoardService` | Publish telemetry/attributes, attr request, server RPC |
| `MetricsTelemetryBridge` | `MetricsCollected` → TB telemetry |

## Telemetry

Topic: `v2/t` (short) or `v1/devices/me/telemetry`.

### 1. Simple key-value (server timestamp)

```cpp
thingsboard::TelemetryBuilder b;
b.add("temperature", 22.5).add("humidity", 61);
tb->publishTelemetry(b);
// → {"temperature":22.5,"humidity":61}
```

### 2. Client-side timestamp

```cpp
thingsboard::TelemetryBuilder b;
b.timestampMs(1451649600512LL)
 .add("temperature", 22.5)
 .add("humidity", 61);
tb->publishTelemetry(b);
// → {"ts":1451649600512,"values":{"temperature":22.5,"humidity":61}}
```

### 3. Batch (array of ts+values)

```cpp
thingsboard::TelemetryBatch batch;
{
    thingsboard::TelemetryBuilder e;
    e.timestampMs(1451649600000LL).add("temperature", 22.5);
    batch.add(std::move(e));
}
{
    thingsboard::TelemetryBuilder e;
    e.timestampMs(1451649601000LL).add("temperature", 22.7);
    batch.add(std::move(e));
}
tb->publishTelemetry(batch);
```

Supported value helpers: `double`, `int64_t`, `bool`, `const char*` / `string_view`, nested JSON via `addRawJson`.

Raw publish still available: `tb->publishTelemetry(R"({"temperature":22.5})")`.

## Metrics bridge

```cpp
registry.createService<embed::MetricsService>();
registry.createService<thingsboard::ThingsBoardService>();
registry.createService<thingsboard::MetricsTelemetryBridge>();
```

## Credentials / service wiring

See earlier README sections — Access Token / MQTT Basic + `ThingsBoardService` in `startAll()`.

## Host tests

```bash
cmake -S host_test -B host_test/build && cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

Includes `test_tb_telemetry` for builder/batch JSON shapes.

## Next

- Attributes API helpers
- Client-side RPC / claim
- Protobuf payloads
