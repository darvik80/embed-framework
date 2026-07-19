#include "alicloud_iot/alicloud_credentials.hpp"
#include <mbedtls/sha256.h>
#include <algorithm>
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
    std::array<uint8_t, SHA256_KEY_IOPAD_SIZE> k_ipad{};
    std::array<uint8_t, SHA256_KEY_IOPAD_SIZE> k_opad{};
    std::array<uint8_t, SHA256_DIGEST_SIZE>    hash{};

    if (msg.empty() || key.empty() || key.size() > SHA256_KEY_IOPAD_SIZE) {
        return false;
    }

    std::copy(key.begin(), key.end(), k_ipad.begin());
    std::copy(key.begin(), key.end(), k_opad.begin());

    for (size_t i = 0; i < SHA256_KEY_IOPAD_SIZE; ++i) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_ipad.data(), SHA256_KEY_IOPAD_SIZE);
    mbedtls_sha256_update(&ctx,
        reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
    mbedtls_sha256_finish(&ctx, hash.data());

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_opad.data(), SHA256_KEY_IOPAD_SIZE);
    mbedtls_sha256_update(&ctx, hash.data(), SHA256_DIGEST_SIZE);
    mbedtls_sha256_finish(&ctx, output.data());
    mbedtls_sha256_free(&ctx);

    return true;
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
