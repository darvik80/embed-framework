# embed_core

WiFi, MQTT, and metrics services built on `embed`.

## Services

| Service | Signals | Notes |
|---------|---------|-------|
| `WifiService` | `onConnected`, `onDisconnected` | STA + CRTP state machine; NVS init in `start()` |
| `MqttService` | `onConnected`, `onDisconnected`, `onMessage` | Connects when WiFi is up; credentials via `MqttCredentials&` |
| `MetricsService` | `onMetricsCollected` | Periodic heap/CPU/WiFi/storage + custom metrics |

## MQTT

- Pass a long-lived `MqttCredentials` implementation (`PlainMqttCredentials`, `AlicloudCredentials`, …).
- Reconnect is **only** via the service state machine + timer (`CONFIG_EMBED_MQTT_*`). esp-mqtt auto-reconnect is off.
- Each Connecting attempt **destroys and recreates** the esp-mqtt client — required because with `disable_auto_reconnect` a disconnected client stays "started" and a second `esp_mqtt_client_start()` fails (`Client has started`).
- `MqttMessageReceived::payload` is `embed::string<767>`; longer payloads truncate with `ESP_LOGW`.

## Metrics storage

Looks up SPIFFS partition label `storage`. When SPIFFS is mounted, `esp_spiffs_info` fills total/used; otherwise only partition total is set.
