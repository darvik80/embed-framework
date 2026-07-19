//
// Created by darvik on 19/7/2026.
//

#pragma once

namespace thingsboard::iot {

/**
 * @brief Alibaba Cloud IoT MQTT credentials.
 *
 * Generates MQTT username, password (HMAC-SHA256 signature), client ID,
 * and broker URI from a product key, device name, and device secret.
 *
 * Implements embed::MqttCredentials so it can be passed directly to
 * embed::MqttService.
 */
class ThingsBoardCredentials : public embed::MqttCredentials {
public:
    ThingsBoardCredentials() = default;

    /**
     * @brief Factory: create credentials from product key, device name, and secret.
     *
     * Returns nullopt if any input is empty/too long or HMAC computation fails.
     */
    static std::optional<ThingsBoardCredentials> create(std::string_view token);

    // embed::MqttCredentials interface
    const char* brokerUri()  const override { return uri_.c_str(); }
    const char* clientId()   const override { return clientId_.c_str(); }
    const char* username()   const override { return username_.c_str(); }
    const char* password()   const override { return password_.c_str(); }
    // cert() not overridden — MqttService uses esp_crt_bundle for wss:// URIs

    [[nodiscard]] bool isValid() const {
        return !username_.empty() && !password_.empty() &&
               !clientId_.empty() && !uri_.empty();
    }

    [[nodiscard]] std::string_view productKey()  const noexcept { return productKey_; }
    [[nodiscard]] std::string_view deviceName()  const noexcept { return deviceName_; }

private:
    std::string username_;
    std::string password_;
    std::string clientId_;
    std::string uri_;
    std::string productKey_;
    std::string deviceName_;

    AlicloudCredentials(std::string_view username,
                        std::string_view password,
                        std::string_view clientId,
                        std::string_view uri,
                        std::string_view productKey,
                        std::string_view deviceName)
        : username_(username), password_(password), clientId_(clientId)
        , uri_(uri), productKey_(productKey), deviceName_(deviceName) {}
};

} // namespace thinsboard::iot
