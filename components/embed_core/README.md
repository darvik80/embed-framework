# embed_core

WiFi, MQTT, and metrics services built on `embed`.

## Services

| Service | Signals | Notes |
|---------|---------|-------|
| `WifiService` | `onConnected`, `onDisconnected` | STA or SoftAP (`enableSoftAp()`); SSID from NVS (`fctry`) with Kconfig seed |
| `MqttService` | `onConnected`, `onDisconnected`, `onMessage` | Connects when WiFi is up; credentials via `MqttCredentials&` |
| `MetricsService` | `onMetricsCollected` | Periodic CPU, heap (all 8-bit), **DRAM**, PSRAM, WiFi, storage + custom |
| `ConfigPortalService` | — | HTTP setup UI (SoftAP `192.168.4.1` and optional STA) |
| `NvsStore` / `device_settings` | — | `fctry` KV; factory reset + portal flag |
| `firmware_slot` | — | OTA slot info, rollback, crash-loop auto-rollback |

## MQTT

- Pass a long-lived `MqttCredentials` implementation (`CreartsCredentials`, `PlainMqttCredentials`, `AlicloudCredentials`, …).
- Reconnect is **only** via the service state machine + timer (`CONFIG_EMBED_MQTT_*`). esp-mqtt auto-reconnect is off.
- `MqttMessageReceived::payload` is `embed::string<1400>`; longer payloads truncate with `ESP_LOGW`.

## Persistent settings

`NvsStore::initFlash()` opens default `nvs` (WiFi PHY) and, if present, **`fctry`**. Device WiFi + cloud identity should live in `fctry` so `idf.py flash` / OTA do not wipe them. First boot seeds from Kconfig.

`factoryResetSettings()` wipes active wifi/crearts (backup `wifi_b` / `crearts_b` is kept) and sets the portal flag so the next boot **does not** re-seed from Kconfig. Hold **BOOT (GPIO 0) 3 s while the app is running**, or press **EN/RST 3× quickly**. RPC `factory_reset` / `config_portal`, or the web UI (**Save** snapshots backup, **Restore** swaps). SoftAP SSID is `{prefix}-{MAC}` (default `embed-A1B2`).

## Metrics storage

Looks up SPIFFS partition label `storage`. When SPIFFS is mounted, `esp_spiffs_info` fills total/used; otherwise only partition total is set.
