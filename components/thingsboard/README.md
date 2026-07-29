# thingsboard

ThingsBoard MQTT device API for embed-framework.

Docs: [Getting Connected](https://thingsboard.io/docs/reference/mqtt-api/getting-connected/)

## Features (v1)

| Piece | Role |
|-------|------|
| `ThingsBoardCredentials` | Access Token + MQTT Basic → `embed::MqttCredentials` |
| `Topics` | Short (`v2/...`) and Standard (`v1/devices/me/...`) topic helpers |
| `ThingsBoardService` | Telemetry / attributes / attribute request / server RPC |

## Credentials

**Access Token** (simplest):

```cpp
static auto creds = thingsboard::ThingsBoardCredentials::createAccessToken(
    "thingsboard.cloud",   // host, no scheme
    "YOUR_ACCESS_TOKEN",
    false,                 // TLS → mqtts:// :8883
    0                      // port 0 = default
);
registry.createService<embed::MqttService>(*creds);
```

MQTT username = access token, password empty, clientId auto from Wi‑Fi MAC.

**MQTT Basic**:

```cpp
static auto creds = thingsboard::ThingsBoardCredentials::createBasic(
    "localhost", "user", "pass", "client-1", false, 1883);
```

## Service usage

```cpp
registry.createService<embed::WifiService>();
registry.createService<embed::MqttService>(*creds);
auto* tb = registry.createService<thingsboard::ThingsBoardService>(
    thingsboard::TopicStyle::Short);
registry.startAll();

// after connect:
tb->publishTelemetry(R"({"temperature":25.1,"humidity":40})");
tb->publishAttributes(R"({"firmwareVersion":"1.0.0"})");
```

Signals: `onAttributeUpdate`, `onRpcRequest`, `onAttributeResponse`.

Respond to RPC:

```cpp
tb->respondRpc(req.requestId, R"({"success":true})");
```

## Kconfig

`menuconfig` → **Embed Framework — ThingsBoard**: host, access token, TLS, port, short topics.

## Next steps

- Client-side RPC (`v2/r/req/$id`) helper
- Device claiming (`v2/c`)
- Optional Protobuf payloads
- Metrics → telemetry bridge service
