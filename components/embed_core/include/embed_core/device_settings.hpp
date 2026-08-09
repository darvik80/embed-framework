#pragma once

#include "esp_err.h"

#include <cstdint>

namespace embed {

struct WifiSettings {
    char ssid[33]{};
    char password[65]{};
};

struct CreartsSettings {
    char product[64]{};
    char device[64]{};
    char host[128]{};
    char token[256]{};
    uint16_t port = 0;
    bool useTls = false;
    bool topicShort = false;
};

[[nodiscard]] bool loadWifiSettings(WifiSettings& out);
esp_err_t saveWifiSettings(const WifiSettings& in);
[[nodiscard]] bool wifiSettingsPresent();

[[nodiscard]] bool loadCreartsSettings(CreartsSettings& out);
esp_err_t saveCreartsSettings(const CreartsSettings& in);
[[nodiscard]] bool creartsSettingsComplete(const CreartsSettings& s);

/// Snapshot active WiFi + Crearts into backup NVS (`wifi_b` / `crearts_b`).
esp_err_t backupSettings();
[[nodiscard]] bool settingsBackupPresent();
[[nodiscard]] bool loadWifiBackup(WifiSettings& out);
[[nodiscard]] bool loadCreartsBackup(CreartsSettings& out);
/// Restore backup → active (previous active becomes the new backup).
esp_err_t restoreSettingsBackup();

/// Overlay JSON onto `wifi` / `crearts` (already loaded values are kept when a
/// key is missing or empty). Accepts nested `{wifi,crearts}` or flat keys.
/// ESP_ERR_INVALID_ARG = bad JSON or missing ssid / product / device / host / token.
esp_err_t parseCredentialsJson(const char* json, WifiSettings& wifi, CreartsSettings& crearts);

/// Backup active settings, apply JSON, clear portal flag. Does not reboot.
esp_err_t importCredentialsJson(const char* json);

/// Write pretty JSON (`wifi` + `crearts`). Token/password omitted unless `includeSecrets`.
esp_err_t exportCredentialsJson(char* out, size_t outLen, bool includeSecrets = true);

[[nodiscard]] bool isConfigPortalRequested();
esp_err_t setConfigPortalRequested(bool on);

/// True if portal flag is set, or there is no WiFi SSID in NVS and no Kconfig seed.
[[nodiscard]] bool needsConfigPortal();

/// Wipe wifi + crearts identity and force config portal. Does not reboot.
esp_err_t factoryResetSettings();

/// Reboot into config AP without wiping (form stays prefilled).
esp_err_t requestConfigPortal();

/// True if reset GPIO is held low for the full hold time (call early in app_main).
[[nodiscard]] bool factoryResetGpioHeld();

/// Watch reset GPIO anytime while running (GPIO 0 / BOOT is a strap pin —
/// holding it *during* chip reset enters download mode, not the app).
void startFactoryResetGpioWatch();

/// EN/RST is not a GPIO — holding it keeps the chip dead. Count rapid
/// external resets (`ESP_RST_EXT` / `ESP_RST_USB`) instead. Call once after
/// NVS init. True → caller should `factoryResetSettings()`. Starts a timer
/// that clears the press counter after a stable boot.
[[nodiscard]] bool checkRstBurstFactoryReset();

void scheduleReboot(uint32_t delayMs);

} // namespace embed
