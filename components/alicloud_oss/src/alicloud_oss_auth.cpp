//
// Created by darvik on 29.12.2024.
// Rewritten to use embed::string instead of c_string.
//

#include "alicloud_oss_auth.h"
#include <esp_log.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include <embed/string.hpp>

// Canonical headers for OSS V4 signature (must be sorted)
static const char* s_canonical_headers[] = {
    "content-disposition",
    "content-md5",
    "content-type",
    "host",
    "x-oss-content-sha256",
    "x-oss-date",
};

// Additional headers for signature
static const char* s_additional_headers[] = {
    "content-disposition",
    "content-md5",
    "content-type",
    "host",
};

// Cryptographic constants
#define SHA256_KEY_IOPAD_SIZE 64
#define SHA256_DIGEST_SIZE    32
#define MAX_STRING_TO_SIGN    512
#define MAX_DATETIME_LEN      32

// OSS header names
#define OSS_HEADER_DATE          "x-oss-date"
#define OSS_HEADER_CONTENT_SHA   "x-oss-content-sha256"
#define OSS_HEADER_AUTH          "Authorization"
#define OSS_UNSIGNED_PAYLOAD     "UNSIGNED-PAYLOAD"
#define OSS_ALGORITHM            "OSS4-HMAC-SHA256"
#define OSS_REQUEST_TYPE         "aliyun_v4_request"

static const char *TAG = "OSS_AUTH";

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Encode binary data to hexadecimal string
 */
static void encode_hex(char *dest, const uint8_t *src, size_t srclen) {
    static const char hex_table[] = "0123456789abcdef";
    for (size_t i = 0; i < srclen; i++) {
        *dest++ = hex_table[src[i] >> 4];
        *dest++ = hex_table[src[i] & 0xf];
    }
    *dest = '\0';
}

/**
 * Compute HMAC-SHA256
 */
static void hmac_sha256(const uint8_t *key, size_t key_len,
                        const char *msg, size_t msg_len,
                        uint8_t output[SHA256_DIGEST_SIZE]) {
    uint8_t k_ipad[SHA256_KEY_IOPAD_SIZE] = {0};
    uint8_t k_opad[SHA256_KEY_IOPAD_SIZE] = {0};

    memcpy(k_ipad, key, key_len);
    memcpy(k_opad, key, key_len);

    for (int i = 0; i < SHA256_KEY_IOPAD_SIZE; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_ipad, SHA256_KEY_IOPAD_SIZE);
    mbedtls_sha256_update(&ctx, (const uint8_t *)msg, msg_len);
    mbedtls_sha256_finish(&ctx, output);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_opad, SHA256_KEY_IOPAD_SIZE);
    mbedtls_sha256_update(&ctx, output, SHA256_DIGEST_SIZE);
    mbedtls_sha256_finish(&ctx, output);
    mbedtls_sha256_free(&ctx);
}

/**
 * Compute SHA256 hash
 */
static void compute_sha256(const char *msg, size_t len, uint8_t output[SHA256_DIGEST_SIZE]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t *)msg, len);
    mbedtls_sha256_finish(&ctx, output);
    mbedtls_sha256_free(&ctx);
}

/**
 * Format current time as ISO8601 (YYYYMMDDTHHMMSSZ)
 */
static void format_iso8601_time(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    snprintf(buffer, buffer_size, "%.4d%.2d%.2dT%.2d%.2d%.2dZ",
             1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/**
 * Percent-encode a path string, keeping '/' and unreserved chars as-is.
 * Unreserved: A-Z a-z 0-9 - _ . ~
 */
static embed::string<512> url_encode_path(const char *path) {
    embed::string<512> result{};
    if (!path) return result;

    static const char hex[] = "0123456789ABCDEF";
    char encoded[4] = {'%', 0, 0, 0};

    for (const char *p = path; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '/' ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[0] = (char)c;
            encoded[1] = '\0';
            result.append(encoded);
        } else {
            encoded[0] = '%';
            encoded[1] = hex[c >> 4];
            encoded[2] = hex[c & 0xf];
            encoded[3] = '\0';
            result.append(encoded);
        }
    }
    return result;
}

/**
 * Build semicolon-separated list of additional headers present in request
 */
static embed::string<128> build_additional_headers_list(http_headers_t *headers) {
    embed::string<128> result{};
    bool first = true;
    size_t count = sizeof(s_additional_headers) / sizeof(s_additional_headers[0]);

    for (size_t i = 0; i < count; i++) {
        if (http_headers_get(headers, s_additional_headers[i])) {
            if (!first) result.append(";");
            result.append(s_additional_headers[i]);
            first = false;
        }
    }
    return result;
}

// ============================================================================
// Canonical Request Building
// ============================================================================

/**
 * Build canonical request for OSS V4 signature
 */
static embed::string<2048> build_canonical_request(alicloud_oss_config_t *config,
                                                    http_method_t method,
                                                    uri_t *uri,
                                                    http_headers_t *headers) {
    embed::string<2048> buf{};

    // 1. HTTP Method
    buf.append(http_method_to_string(method));
    buf.append("\n");

    // 2. Canonical URI (URL-encoded path)
    buf.append("/");
    if (*config->bucket) {
        buf.append(config->bucket);
    }
    embed::string<512> encoded_path = url_encode_path(uri->path);
    buf.append(encoded_path.c_str());
    buf.append("\n");

    // 3. Canonical Query String
    char *query = query_params_encode(uri_get_params(uri));
    if (query) {
        buf.append(query);
        free(query);
    }
    buf.append("\n");

    // 4. Canonical Headers (sorted, lowercase)
    size_t header_count = sizeof(s_canonical_headers) / sizeof(s_canonical_headers[0]);
    for (size_t i = 0; i < header_count; i++) {
        const char *value = http_headers_get(headers, s_canonical_headers[i]);
        if (value) {
            buf.append(s_canonical_headers[i]);
            buf.append(":");
            buf.append(value);
            buf.append("\n");
        }
    }
    buf.append("\n");

    // 5. Additional Headers List
    embed::string<128> additional = build_additional_headers_list(headers);
    buf.append(additional.c_str());
    buf.append("\n");

    // 6. Hashed Payload
    buf.append(OSS_UNSIGNED_PAYLOAD);

    ESP_LOGD(TAG, "Canonical request built");
    return buf;
}

// ============================================================================
// Signature Building
// ============================================================================

/**
 * Build string to sign: Algorithm\nDateTime\nScope\nHashedCanonicalRequest
 */
static embed::string<MAX_STRING_TO_SIGN> build_string_to_sign(const char *datetime,
                                                               const char *date,
                                                               const char *region,
                                                               const char *canonical_request) {
    uint8_t hash[SHA256_DIGEST_SIZE];
    char hex[SHA256_DIGEST_SIZE * 2 + 1];

    compute_sha256(canonical_request, strlen(canonical_request), hash);
    encode_hex(hex, hash, SHA256_DIGEST_SIZE);

    char tmp[MAX_STRING_TO_SIGN];
    snprintf(tmp, sizeof(tmp),
             "%s\n%s\n%s/%s/oss/%s\n%s",
             OSS_ALGORITHM, datetime, date, region, OSS_REQUEST_TYPE, hex);

    return embed::string<MAX_STRING_TO_SIGN>{tmp};
}

/**
 * Build signing key: HMAC chain of date -> region -> product -> request_type
 */
static void build_signing_key(const char *secret, const char *date,
                               const char *region,
                               uint8_t signing_key[SHA256_DIGEST_SIZE]) {
    char secret_key[64];
    uint8_t k_date[SHA256_DIGEST_SIZE];
    uint8_t k_region[SHA256_DIGEST_SIZE];
    uint8_t k_service[SHA256_DIGEST_SIZE];

    snprintf(secret_key, sizeof(secret_key), "aliyun_v4%s", secret);

    hmac_sha256((uint8_t*)secret_key, strlen(secret_key), date, strlen(date), k_date);
    hmac_sha256(k_date, SHA256_DIGEST_SIZE, region, strlen(region), k_region);
    hmac_sha256(k_region, SHA256_DIGEST_SIZE, "oss", 3, k_service);
    hmac_sha256(k_service, SHA256_DIGEST_SIZE, OSS_REQUEST_TYPE, strlen(OSS_REQUEST_TYPE), signing_key);
}

/**
 * Build final signature hex string from signing key and string to sign
 */
static embed::string<SHA256_DIGEST_SIZE * 2 + 1> build_signature(
        const uint8_t signing_key[SHA256_DIGEST_SIZE],
        const char *string_to_sign) {
    uint8_t signature[SHA256_DIGEST_SIZE];
    char hex[SHA256_DIGEST_SIZE * 2 + 1];

    hmac_sha256(signing_key, SHA256_DIGEST_SIZE,
                string_to_sign, strlen(string_to_sign), signature);
    encode_hex(hex, signature, SHA256_DIGEST_SIZE);

    return embed::string<SHA256_DIGEST_SIZE * 2 + 1>{hex};
}

// ============================================================================
// Public API
// ============================================================================

/**
 * Sign OSS request using V4 signature algorithm
 */
esp_err_t alicloud_oss_sign_v4(alicloud_oss_config_t *config,
                                http_method_t method,
                                uri_t *uri,
                                http_headers_t *headers) {
    // Sort for canonical form
    query_params_sort(uri_get_params(uri));
    http_headers_sort(headers);

    // Generate timestamp
    char datetime[MAX_DATETIME_LEN];
    char date[9]; // YYYYMMDD + null
    format_iso8601_time(datetime, sizeof(datetime));
    memcpy(date, datetime, 8);
    date[8] = '\0';

    // Set required headers
    http_headers_set(headers, OSS_HEADER_CONTENT_SHA, OSS_UNSIGNED_PAYLOAD);
    http_headers_set(headers, OSS_HEADER_DATE, datetime);

    // Build canonical request
    embed::string<2048> canonical_req = build_canonical_request(config, method, uri, headers);

    // Build string to sign
    embed::string<MAX_STRING_TO_SIGN> str_to_sign = build_string_to_sign(
        datetime, date, config->region, canonical_req.c_str());

    // Build signing key and signature
    uint8_t signing_key[SHA256_DIGEST_SIZE];
    build_signing_key(config->access_key_secret, date, config->region, signing_key);

    embed::string<SHA256_DIGEST_SIZE * 2 + 1> signature = build_signature(
        signing_key, str_to_sign.c_str());

    // Build authorization header
    embed::string<128> additional = build_additional_headers_list(headers);

    // OSS4-HMAC-SHA256 Credential=<id>/<date>/<region>/oss/aliyun_v4_request,
    //                  AdditionalHeaders=<list>,Signature=<hex>
    char auth_buf[512];
    snprintf(auth_buf, sizeof(auth_buf),
             "%s Credential=%s/%s/%s/oss/%s,AdditionalHeaders=%s,Signature=%s",
             OSS_ALGORITHM, config->access_key_id, date, config->region,
             OSS_REQUEST_TYPE, additional.c_str(), signature.c_str());

    http_headers_set(headers, OSS_HEADER_AUTH, auth_buf);

    ESP_LOGD(TAG, "Request signed successfully");
    return ESP_OK;
}
