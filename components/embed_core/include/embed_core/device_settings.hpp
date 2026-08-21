#pragma once

#include "esp_err.h"

#include <cstdint>

namespace embed {

struct WifiSettings {
    char ssid[33]{};
    char password[65]{};
};

[[nodiscard]] bool loadWifiSettings(WifiSettings& out);
esp_err_t saveWifiSettings(const WifiSettings& in);
[[nodiscard]] bool wifiSettingsPresent();

[[nodiscard]] bool loadWifiBackup(WifiSettings& out);
esp_err_t saveWifiBackup(const WifiSettings& in);
esp_err_t eraseWifiSettings();

/// Snapshot active WiFi into `wifi_b`.
esp_err_t backupWifiSettings();

[[nodiscard]] bool isConfigPortalRequested();
esp_err_t setConfigPortalRequested(bool on);

/// True if portal flag is set, or there is no WiFi SSID in NVS and no Kconfig seed.
[[nodiscard]] bool needsConfigPortal();

/// Default: wipe WiFi + force portal. Cloud providers may override via setFactoryResetHandler.
esp_err_t factoryResetSettings();

using FactoryResetHandler = esp_err_t (*)();
void setFactoryResetHandler(FactoryResetHandler handler);

/// Reboot into config AP without wiping (form stays prefilled).
esp_err_t requestConfigPortal();

[[nodiscard]] bool factoryResetGpioHeld();
void startFactoryResetGpioWatch();

/// EN/RST burst detector. True → caller should run factoryResetSettings().
[[nodiscard]] bool checkRstBurstFactoryReset();

void scheduleReboot(uint32_t delayMs);

} // namespace embed
