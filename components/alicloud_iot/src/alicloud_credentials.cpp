#include "alicloud_iot/alicloud_credentials.hpp"
#include "embed/crypto.hpp"
#include <format>


namespace alicloud::iot {

namespace {

constexpr std::string_view TIMESTAMP_VALUE = "2524608000000";

constexpr std::string_view MQTT_CLIENTID_KV =
    "|timestamp=2524608000000,ext=3,_ss=2,_v=1.0.0,_m=ESP32-S3,securemode=3,signmethod=hmacsha256,lan=cpp|";

constexpr size_t SHA256_KEY_IOPAD_SIZE = 64;
constexpr size_t SHA256_DIGEST_SIZE    = 32;

bool hmac_sha256(std::string_view msg, std::string_view key,
                 std::array<uint8_t, SHA256_DIGEST_SIZE>& output)
{
    if (msg.empty() || key.empty() || key.size() > SHA256_KEY_IOPAD_SIZE) {
        return false;
    }

    return embed::crypto::hmacSha256(
        reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(),
        output.data());
}

std::string bytes_to_hex(const std::array<uint8_t, SHA256_DIGEST_SIZE>& input)
{
    std::string result;
    result.reserve(SHA256_DIGEST_SIZE * 2);
    constexpr std::string_view hex_chars = "0123456789ABCDEF";
    for (uint8_t b : input) {
        result.push_back(hex_chars[(b >> 4) & 0x0F]);
        result.push_back(hex_chars[b & 0x0F]);
    }
    return result;
}

} // anonymous namespace

// ── Factory ─────────────────────────────────────────────────────────────

std::optional<AlicloudCredentials>
AlicloudCredentials::create(std::string_view product,
                             std::string_view device,
                             std::string_view secret)
{
    if (product.empty() || product.size() >= PRODUCT_KEY_MAX_LEN ||
        device.empty()  || device.size()  >= DEVICE_NAME_MAX_LEN ||
        secret.empty())
    {
        return std::nullopt;
    }

    // username: device&product
    std::string username = std::format("{}&{}", device, product);
    if (username.size() >= USERNAME_MAX_LEN) return std::nullopt;

    // password: HMAC-SHA256( clientId<username>deviceName<device>productKey<product>timestamp<ts>, secret )
    std::string data = std::format("clientId{}deviceName{}productKey{}timestamp{}",
                                   username, device, product, TIMESTAMP_VALUE);

    std::array<uint8_t, SHA256_DIGEST_SIZE> mac{};
    if (!hmac_sha256(data, secret, mac)) return std::nullopt;

    std::string password = bytes_to_hex(mac);
    if (password.size() >= PASSWORD_MAX_LEN) return std::nullopt;

    // clientId: username + KV
    std::string clientId = std::format("{}{}", username, MQTT_CLIENTID_KV);
    if (clientId.size() >= CLIENTID_MAX_LEN) return std::nullopt;

    // URI: wss://{product}.iot-as-mqtt.ap-southeast-1.aliyuncs.com
    std::string uri = std::format("wss://{}.iot-as-mqtt.ap-southeast-1.aliyuncs.com", product);
    if (uri.size() >= URI_MAX_LEN) return std::nullopt;

    return AlicloudCredentials(username, password, clientId, uri, product, device);
}

} // namespace alicloud::iot
