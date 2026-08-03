#include "crearts_iot/credentials.hpp"

#include "esp_log.h"

#include <format>

namespace crearts::iot {

namespace {

constexpr size_t kIdMax = 63;
constexpr size_t kHostMax = 128;
constexpr size_t kUserMax = 256;
constexpr size_t kPassMax = 256;
constexpr size_t kTokenMax = 256;
constexpr size_t kUriMax = 196;

const char* TAG = "CreartsCreds";

std::string makeUri(std::string_view host, bool useTls, uint16_t port)
{
    if (port == 0) {
        port = useTls ? 8883 : 1883;
    }
    return std::format("{}://{}:{}", useTls ? "mqtts" : "mqtt", host, port);
}

bool idOk(std::string_view id)
{
    if (id.empty() || id.size() > kIdMax) {
        return false;
    }
    if (id.find('/') != std::string_view::npos) {
        return false;
    }
    const char c0 = id.front();
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z') ||
          (c0 >= '0' && c0 <= '9'))) {
        return false;
    }
    return true;
}

bool hostOk(std::string_view host)
{
    return !host.empty() && host.size() < kHostMax &&
           host.find("://") == std::string_view::npos;
}

std::optional<CreartsCredentials> build(std::string_view productId,
                                        std::string_view deviceId,
                                        std::string_view host,
                                        std::string_view username,
                                        std::string_view password,
                                        TopicStyle style,
                                        bool useTls,
                                        uint16_t port,
                                        bool accessTokenAuth)
{
    if (!idOk(productId) || !idOk(deviceId)) {
        ESP_LOGE(TAG, "Invalid product_id or device_id");
        return std::nullopt;
    }
    if (!hostOk(host)) {
        ESP_LOGE(TAG, "Invalid host");
        return std::nullopt;
    }
    if (username.empty() || username.size() >= kUserMax) {
        ESP_LOGE(TAG, "Invalid username/token");
        return std::nullopt;
    }
    if (password.size() >= kPassMax) {
        return std::nullopt;
    }

    std::string uri = makeUri(host, useTls, port);
    if (uri.size() >= kUriMax) {
        return std::nullopt;
    }

    std::string clientId = std::format("{}.{}", productId, deviceId);
    Topics topics(productId, deviceId, style);
    std::string willTopic = topics.statusPublish();
    std::string willMessage = CreartsCredentials::makeOfflineStatusJson("lwt");

    return CreartsCredentials(std::string(productId),
                              std::string(deviceId),
                              std::string(username),
                              std::string(password),
                              std::move(clientId),
                              std::move(uri),
                              std::string(host),
                              std::move(willTopic),
                              std::move(willMessage),
                              style,
                              useTls,
                              accessTokenAuth);
}

} // namespace

std::string CreartsCredentials::makeOfflineStatusJson(const char* reason)
{
    return std::format(R"({{"online":false,"ts":0,"reason":"{}"}})",
                       reason ? reason : "lwt");
}

CreartsCredentials::CreartsCredentials(std::string productId,
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
                                       bool accessTokenAuth)
    : productId_(std::move(productId))
    , deviceId_(std::move(deviceId))
    , username_(std::move(username))
    , password_(std::move(password))
    , clientId_(std::move(clientId))
    , uri_(std::move(uri))
    , host_(std::move(host))
    , willTopic_(std::move(willTopic))
    , willMessage_(std::move(willMessage))
    , style_(style)
    , useTls_(useTls)
    , accessTokenAuth_(accessTokenAuth)
{}

std::optional<CreartsCredentials> CreartsCredentials::createAccessToken(
    std::string_view productId,
    std::string_view deviceId,
    std::string_view host,
    std::string_view accessToken,
    TopicStyle style,
    bool useTls,
    uint16_t port)
{
    if (accessToken.empty() || accessToken.size() >= kTokenMax) {
        ESP_LOGE(TAG, "Invalid access token");
        return std::nullopt;
    }
    // Username = token. Password = token as well so stock RabbitMQ (no empty
    // passwords) works; brokers that allow empty password still accept this.
    return build(productId, deviceId, host, accessToken, accessToken, style,
                 useTls, port, true);
}

std::optional<CreartsCredentials> CreartsCredentials::createBasic(
    std::string_view productId,
    std::string_view deviceId,
    std::string_view host,
    std::string_view username,
    std::string_view password,
    TopicStyle style,
    bool useTls,
    uint16_t port)
{
    return build(productId, deviceId, host, username, password, style, useTls,
                 port, false);
}

} // namespace crearts::iot
