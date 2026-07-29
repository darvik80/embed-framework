#include "thingsboard/credentials.hpp"

#include "esp_mac.h"
#include "esp_log.h"

#include <cstdio>
#include <format>

namespace thingsboard {

namespace {

constexpr size_t kHostMax = 128;
constexpr size_t kTokenMax = 128;
constexpr size_t kUserMax = 128;
constexpr size_t kPassMax = 128;
constexpr size_t kClientIdMax = 64;
constexpr size_t kUriMax = 196;

const char* TAG = "TbCredentials";

std::string makeUri(std::string_view host, bool useTls, uint16_t port)
{
    if (port == 0) {
        port = useTls ? 8883 : 1883;
    }
    const char* scheme = useTls ? "mqtts" : "mqtt";
    return std::format("{}://{}:{}", scheme, host, port);
}

std::string defaultClientId()
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "esp32s3-%02x%02x%02x%02x%02x%02x",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

bool hostOk(std::string_view host)
{
    return !host.empty() && host.size() < kHostMax &&
           host.find("://") == std::string_view::npos;
}

} // namespace

ThingsBoardCredentials::ThingsBoardCredentials(std::string username,
                                               std::string password,
                                               std::string clientId,
                                               std::string uri,
                                               std::string host,
                                               bool useTls)
    : username_(std::move(username))
    , password_(std::move(password))
    , clientId_(std::move(clientId))
    , uri_(std::move(uri))
    , host_(std::move(host))
    , useTls_(useTls)
{}

std::optional<ThingsBoardCredentials>
ThingsBoardCredentials::createAccessToken(std::string_view host,
                                          std::string_view accessToken,
                                          bool useTls,
                                          uint16_t port,
                                          std::string_view clientId)
{
    if (!hostOk(host) || accessToken.empty() || accessToken.size() >= kTokenMax) {
        ESP_LOGE(TAG, "Invalid host or access token");
        return std::nullopt;
    }

    std::string uri = makeUri(host, useTls, port);
    if (uri.size() >= kUriMax) {
        return std::nullopt;
    }

    std::string cid = clientId.empty() ? defaultClientId()
                                       : std::string(clientId);
    if (cid.size() >= kClientIdMax) {
        return std::nullopt;
    }

    // Access Token: username = token, password empty
    return ThingsBoardCredentials(std::string(accessToken),
                                  {},
                                  std::move(cid),
                                  std::move(uri),
                                  std::string(host),
                                  useTls);
}

std::optional<ThingsBoardCredentials>
ThingsBoardCredentials::createBasic(std::string_view host,
                                    std::string_view username,
                                    std::string_view password,
                                    std::string_view clientId,
                                    bool useTls,
                                    uint16_t port)
{
    if (!hostOk(host)) {
        ESP_LOGE(TAG, "Invalid host");
        return std::nullopt;
    }
    if (username.size() >= kUserMax || password.size() >= kPassMax) {
        return std::nullopt;
    }
    if (username.empty() && clientId.empty()) {
        ESP_LOGE(TAG, "MQTT Basic requires username and/or clientId");
        return std::nullopt;
    }

    std::string uri = makeUri(host, useTls, port);
    if (uri.size() >= kUriMax) {
        return std::nullopt;
    }

    std::string cid = clientId.empty() ? defaultClientId()
                                       : std::string(clientId);
    if (cid.size() >= kClientIdMax) {
        return std::nullopt;
    }

    return ThingsBoardCredentials(std::string(username),
                                  std::string(password),
                                  std::move(cid),
                                  std::move(uri),
                                  std::string(host),
                                  useTls);
}

} // namespace thingsboard
