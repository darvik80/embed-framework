# embed_core

WiFi, MQTT, and metrics services built on `embed`.

## Services

| Service | Signals | Notes |
|---------|---------|-------|
| `WifiService` | `onConnected`, `onDisconnected` | STA or SoftAP (`enableSoftAp()`); SSID from NVS (`fctry`) with Kconfig seed |
| `MqttService` | `onConnected`, `onDisconnected`, `onMessage` | Connects when WiFi is up; credentials via `MqttCredentials&` |
| `MetricsService` | `onMetricsCollected` | Periodic CPU, heap (all 8-bit), **DRAM**, PSRAM, WiFi, storage + custom |
| `NvsStore` / `device_settings` | — | `fctry` KV; WiFi settings + portal flag; factory-reset GPIO/RST helpers |
| `firmware_slot` | — | OTA slot info, rollback, crash-loop auto-rollback |

Cloud provider identity (Cogitor / Alicloud / ThingsBoard) lives in the provider component, not here.

## MQTT

- Pass a long-lived `MqttCredentials` implementation (`CogitorCredentials`, `PlainMqttCredentials`, `AlicloudCredentials`, …).
- Reconnect is **only** via the service state machine + timer (`CONFIG_EMBED_MQTT_*`). esp-mqtt auto-reconnect is off.
- `MqttMessageReceived::payload` is `embed::string<1400>`; longer payloads truncate with `ESP_LOGW`.

## Persistent settings

`NvsStore::initFlash()` opens default `nvs` (WiFi PHY) and, if present, **`fctry`**. Device WiFi should live in `fctry` so `idf.py flash` / OTA do not wipe it. First boot seeds from Kconfig.

`factoryResetSettings()` defaults to wiping active WiFi (`wifi_b` kept) and setting the portal flag. Providers may override via `setFactoryResetHandler` (Cogitor installs a full WiFi+cloud wipe). SoftAP SSID is `{prefix}-{MAC}` (default `embed-A1B2`).

## Metrics storage

Looks up SPIFFS partition label `storage`. When SPIFFS is mounted, `esp_spiffs_info` fills total/used; otherwise only partition total is set.
