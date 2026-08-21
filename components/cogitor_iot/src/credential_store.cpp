#include "cogitor_iot/credential_store.hpp"
#include "cogitor_iot/settings.hpp"

#include "embed_core/device_settings.hpp"

#include "esp_log.h"

#include <cstring>

namespace cogitor::iot {

static const char* TAG = "CogitorCredsNvs";

std::optional<CogitorCredentials> loadOrSeedCredentials(
    std::string_view seedProductId,
    std::string_view seedDeviceId,
    std::string_view seedHost,
    std::string_view seedAccessToken,
    TopicStyle seedStyle,
    bool seedUseTls,
    uint16_t seedPort)
{
    CogitorSettings s{};
    const bool fromNvs = loadSettings(s) && settingsComplete(s);

    if (embed::isConfigPortalRequested() && !fromNvs) {
        ESP_LOGI(TAG, "portal requested — skip Kconfig seed");
        return std::nullopt;
    }

    if (!fromNvs) {
        if (embed::isConfigPortalRequested()) {
            return std::nullopt;
        }
        if (seedProductId.empty() || seedDeviceId.empty() || seedHost.empty() ||
            seedAccessToken.empty()) {
            ESP_LOGE(TAG, "NVS empty and Kconfig seed incomplete");
            return std::nullopt;
        }
        if (seedProductId.size() >= sizeof(s.product) ||
            seedDeviceId.size() >= sizeof(s.device) ||
            seedHost.size() >= sizeof(s.host) ||
            seedAccessToken.size() >= sizeof(s.token)) {
            ESP_LOGE(TAG, "Kconfig seed too long");
            return std::nullopt;
        }
        std::memcpy(s.product, seedProductId.data(), seedProductId.size());
        s.product[seedProductId.size()] = '\0';
        std::memcpy(s.device, seedDeviceId.data(), seedDeviceId.size());
        s.device[seedDeviceId.size()] = '\0';
        std::memcpy(s.host, seedHost.data(), seedHost.size());
        s.host[seedHost.size()] = '\0';
        std::memcpy(s.token, seedAccessToken.data(), seedAccessToken.size());
        s.token[seedAccessToken.size()] = '\0';
        s.port = seedPort;
        s.useTls = seedUseTls;
        s.topicShort = (seedStyle == TopicStyle::Short);

        const esp_err_t err = saveSettings(s);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "seed persist failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "seeded Kconfig → NVS %s.%s @ %s", s.product, s.device, s.host);
        }
    } else {
        ESP_LOGI(TAG, "loaded NVS %s.%s @ %s tls=%d short=%d port=%u",
                 s.product, s.device, s.host, s.useTls ? 1 : 0, s.topicShort ? 1 : 0,
                 static_cast<unsigned>(s.port));
    }

    return CogitorCredentials::createAccessToken(
        s.product, s.device, s.host, s.token,
        s.topicShort ? TopicStyle::Short : TopicStyle::Full, s.useTls, s.port);
}

} // namespace cogitor::iot
