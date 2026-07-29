#pragma once

#include "embed_core/mqtt_credentials.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace thingsboard {

/// MQTT credentials for ThingsBoard.
///
/// Supported modes (see https://thingsboard.io/docs/reference/mqtt-api/getting-connected/):
///   - Access Token — username = device access token, password empty, clientId any
///   - MQTT Basic   — custom username / password / clientId
///
/// Implements embed::MqttCredentials for use with embed::MqttService.
/// Lifetime: must outlive MqttService (typically static in app_main).
class ThingsBoardCredentials : public embed::MqttCredentials {
public:
    ThingsBoardCredentials() = default;

    /// Access Token auth. Port defaults to 1883 (plain) or 8883 (TLS).
    /// clientId may be empty — a stable default is generated.
    static std::optional<ThingsBoardCredentials> createAccessToken(
        std::string_view host,
        std::string_view accessToken,
        bool useTls = false,
        uint16_t port = 0,
        std::string_view clientId = {});

    /// MQTT Basic auth. Any of username/password/clientId may be empty
    /// depending on the scheme configured in ThingsBoard.
    static std::optional<ThingsBoardCredentials> createBasic(
        std::string_view host,
        std::string_view username,
        std::string_view password,
        std::string_view clientId,
        bool useTls = false,
        uint16_t port = 0);

    const char* brokerUri() const override { return uri_.c_str(); }
    const char* clientId()  const override { return clientId_.c_str(); }
    const char* username()  const override { return username_.c_str(); }
    const char* password()  const override { return password_.c_str(); }

    /// Access-token mode: username set, password empty is valid.
    [[nodiscard]] bool isValid() const {
        return !uri_.empty() && (!username_.empty() || !clientId_.empty());
    }

    [[nodiscard]] bool usesTls() const { return useTls_; }
    [[nodiscard]] std::string_view host() const { return host_; }

private:
    std::string username_;
    std::string password_;
    std::string clientId_;
    std::string uri_;
    std::string host_;
    bool useTls_ = false;

    ThingsBoardCredentials(std::string username,
                           std::string password,
                           std::string clientId,
                           std::string uri,
                           std::string host,
                           bool useTls);
};

} // namespace thingsboard
