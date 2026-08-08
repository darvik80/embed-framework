# embed_core

WiFi, MQTT, and metrics services built on `embed`.

## Services

| Service | Signals | Notes |
|---------|---------|-------|
| `WifiService` | `onConnected`, `onDisconnected` | STA + CRTP state machine; NVS init in `start()` |
| `MqttService` | `onConnected`, `onDisconnected`, `onMessage` | Connects when WiFi is up; credentials via `MqttCredentials&` |
| `MetricsService` | `onMetricsCollected` | Periodic CPU, heap (all 8-bit), **DRAM**, PSRAM, WiFi, storage + custom |

## MQTT

- Pass a long-lived `MqttCredentials` implementation (`CreartsCredentials`, `PlainMqttCredentials`, `AlicloudCredentials`, …).
- Reconnect is **only** via the service state machine + timer (`CONFIG_EMBED_MQTT_*`). esp-mqtt auto-reconnect is off.
- `MqttMessageReceived::payload` is `embed::string<767>`; longer payloads truncate with `ESP_LOGW`.

## Metrics storage

Looks up SPIFFS partition label `storage`. When SPIFFS is mounted, `esp_spiffs_info` fills total/used; otherwise only partition total is set.
