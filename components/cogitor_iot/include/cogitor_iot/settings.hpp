#pragma once

#include "embed_core/device_settings.hpp"

#include "esp_err.h"

#include <cstdint>

namespace cogitor::iot {

/// Cogitor MQTT identity in NVS (`fctry`, namespaces `cogitor` / `cogitor_b`).
struct CogitorSettings {
    char product[64]{};
    char device[64]{};
    char host[128]{};
    char token[256]{};
    uint16_t port = 0;
    bool useTls = false;
    bool topicShort = false;
};

[[nodiscard]] bool loadSettings(CogitorSettings& out);
esp_err_t saveSettings(const CogitorSettings& in);
[[nodiscard]] bool settingsComplete(const CogitorSettings& s);
[[nodiscard]] bool loadSettingsBackup(CogitorSettings& out);

/// Snapshot WiFi (embed) + Cogitor into backup namespaces.
esp_err_t backupDeviceSettings();
[[nodiscard]] bool deviceSettingsBackupPresent();
/// Restore backup → active (previous active becomes the new backup).
esp_err_t restoreDeviceSettingsBackup();

/// Overlay JSON onto wifi + cogitor. Accepts `{wifi,cogitor}` (alias `mqtt`) or flat keys.
esp_err_t parseCredentialsJson(const char* json, embed::WifiSettings& wifi, CogitorSettings& cogitor);
esp_err_t importCredentialsJson(const char* json);
esp_err_t exportCredentialsJson(char* out, size_t outLen, bool includeSecrets = true);

/// Wipe WiFi + Cogitor active identity, keep backups, force config portal.
esp_err_t factoryResetSettings();

/// Install as embed factory-reset handler (GPIO / shared default). Call once early.
void installFactoryResetHandler();

} // namespace cogitor::iot
