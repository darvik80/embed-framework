#pragma once

#include "embed_core/mqtt_credentials.hpp"
#include "crearts_iot/topics.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace crearts::iot {

/// MQTT credentials for Crearts IoT Platform.
///
/// Primary auth: dashboard-issued **access token**
///   - MQTT username = access token
///   - MQTT password = access token (RabbitMQ-friendly; empty also allowed by brokers that permit it)
///   - MQTT client id = `{product_id}.{device_id}`
///
/// Configures LWT on `up/status` / `v1/s` (retained offline JSON).
/// Lifetime: must outlive MqttService (typically static in app_main).
class CreartsCredentials : public embed::MqttCredentials {
public:
    CreartsCredentials() = default;

    /// Production path: token from dashboard device registration.
    static std::optional<CreartsCredentials> createAccessToken(
        std::string_view productId,
        std::string_view deviceId,
        std::string_view host,
        std::string_view accessToken,
        TopicStyle style = TopicStyle::Short,
        bool useTls = false,
        uint16_t port = 0);

    /// Lab / custom broker: explicit username + password.
    /// Prefer createAccessToken() for Crearts platform devices.
    static std::optional<CreartsCredentials> createBasic(
        std::string_view productId,
        std::string_view deviceId,
        std::string_view host,
        std::string_view username,
        std::string_view password,
        TopicStyle style = TopicStyle::Short,
        bool useTls = false,
        uint16_t port = 0);

    const char* brokerUri() const override { return uri_.c_str(); }
    const char* clientId() const override { return clientId_.c_str(); }
    const char* username() const override { return username_.c_str(); }
    const char* password() const override { return password_.c_str(); }

    const char* willTopic() const override { return willTopic_.c_str(); }
    const char* willMessage() const override { return willMessage_.c_str(); }
    size_t willMessageLen() const override { return willMessage_.size(); }
    int willQos() const override { return 1; }
    bool willRetain() const override { return true; }

    [[nodiscard]] bool isValid() const {
        return !uri_.empty() && !productId_.empty() && !deviceId_.empty() &&
               !username_.empty();
    }

    [[nodiscard]] bool usesTls() const { return useTls_; }
    [[nodiscard]] bool usesAccessToken() const { return accessTokenAuth_; }
    [[nodiscard]] TopicStyle topicStyle() const { return style_; }
    [[nodiscard]] std::string_view productId() const { return productId_; }
    [[nodiscard]] std::string_view deviceId() const { return deviceId_; }
    [[nodiscard]] std::string_view host() const { return host_; }

    [[nodiscard]] static std::string makeOfflineStatusJson(const char* reason = "lwt");

private:
    std::string productId_;
    std::string deviceId_;
    std::string username_;
    std::string password_;
    std::string clientId_;
    std::string uri_;
    std::string host_;
    std::string willTopic_;
    std::string willMessage_;
    TopicStyle style_ = TopicStyle::Short;
    bool useTls_ = false;
    bool accessTokenAuth_ = false;

    CreartsCredentials(std::string productId,
                       std::string deviceId,
                       std::string username,
                       std::string password,
                       std::string clientId,
                       std::string uri,
                       std::string host,
                       std::string willTopic,
                       std::string willMessage,
                       TopicStyle style,
                       bool useTls,
                       bool accessTokenAuth);
};

} // namespace crearts::iot
