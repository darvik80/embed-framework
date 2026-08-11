#include "embed/crypto.hpp"

#include "esp_idf_version.h"
#include "esp_log.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "psa/crypto.h"
#define EMBED_CRYPTO_PSA 1
#else
#include "mbedtls/md5.h"
#include "mbedtls/sha256.h"
#define EMBED_CRYPTO_PSA 0
#endif

#include <cstring>
#include <new>

namespace embed::crypto {

static const char* TAG = "Crypto";

namespace {

#if EMBED_CRYPTO_PSA
bool psaOk(psa_status_t status, const char* what)
{
    if (status == PSA_SUCCESS) {
        return true;
    }
    ESP_LOGE(TAG, "%s failed: %d", what, static_cast<int>(status));
    return false;
}
#endif

} // namespace

// ── Sha256 ────────────────────────────────────────────────────────────────

struct Sha256::Impl {
#if EMBED_CRYPTO_PSA
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    bool active = false;

    bool start()
    {
        op = PSA_HASH_OPERATION_INIT;
        active = psaOk(psa_hash_setup(&op, PSA_ALG_SHA_256), "sha256 setup");
        return active;
    }

    bool update(const uint8_t* data, size_t len)
    {
        if (!active) {
            return false;
        }
        return psaOk(psa_hash_update(&op, data, len), "sha256 update");
    }

    bool finish(uint8_t out[32])
    {
        if (!active) {
            return false;
        }
        size_t outLen = 0;
        const bool ok = psaOk(psa_hash_finish(&op, out, 32, &outLen), "sha256 finish") &&
                        outLen == 32;
        active = false;
        op = PSA_HASH_OPERATION_INIT;
        return ok;
    }

    void abort()
    {
        if (active) {
            psa_hash_abort(&op);
            active = false;
        }
        op = PSA_HASH_OPERATION_INIT;
    }
#else
    mbedtls_sha256_context ctx{};

    Impl() { mbedtls_sha256_init(&ctx); }

    ~Impl() { mbedtls_sha256_free(&ctx); }

    bool start() { return mbedtls_sha256_starts(&ctx, 0) == 0; }

    bool update(const uint8_t* data, size_t len)
    {
        return mbedtls_sha256_update(&ctx, data, len) == 0;
    }

    bool finish(uint8_t out[32]) { return mbedtls_sha256_finish(&ctx, out) == 0; }

    void abort() {}
#endif
};

Sha256::Sha256() : impl_(new (std::nothrow) Impl())
{
    if (impl_ && !impl_->start()) {
        delete impl_;
        impl_ = nullptr;
    }
}

Sha256::~Sha256()
{
    if (impl_) {
        impl_->abort();
        delete impl_;
    }
}

Sha256::Sha256(Sha256&& other) noexcept : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

Sha256& Sha256::operator=(Sha256&& other) noexcept
{
    if (this != &other) {
        if (impl_) {
            impl_->abort();
            delete impl_;
        }
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

bool Sha256::update(const uint8_t* data, size_t len)
{
    return impl_ && impl_->update(data, len);
}

bool Sha256::finish(uint8_t out[32])
{
    return impl_ && impl_->finish(out);
}

// ── Md5 ─────────────────────────────────────────────────────────────────

struct Md5::Impl {
#if EMBED_CRYPTO_PSA
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    bool active = false;

    bool start()
    {
        op = PSA_HASH_OPERATION_INIT;
        active = psaOk(psa_hash_setup(&op, PSA_ALG_MD5), "md5 setup");
        return active;
    }

    bool update(const uint8_t* data, size_t len)
    {
        if (!active) {
            return false;
        }
        return psaOk(psa_hash_update(&op, data, len), "md5 update");
    }

    bool finish(uint8_t out[16])
    {
        if (!active) {
            return false;
        }
        size_t outLen = 0;
        const bool ok = psaOk(psa_hash_finish(&op, out, 16, &outLen), "md5 finish") &&
                        outLen == 16;
        active = false;
        op = PSA_HASH_OPERATION_INIT;
        return ok;
    }

    void abort()
    {
        if (active) {
            psa_hash_abort(&op);
            active = false;
        }
        op = PSA_HASH_OPERATION_INIT;
    }
#else
    mbedtls_md5_context ctx{};

    Impl() { mbedtls_md5_init(&ctx); }

    ~Impl() { mbedtls_md5_free(&ctx); }

    bool start() { return mbedtls_md5_starts(&ctx) == 0; }

    bool update(const uint8_t* data, size_t len)
    {
        return mbedtls_md5_update(&ctx, data, len) == 0;
    }

    bool finish(uint8_t out[16]) { return mbedtls_md5_finish(&ctx, out) == 0; }

    void abort() {}
#endif
};

Md5::Md5() : impl_(new (std::nothrow) Impl())
{
    if (impl_ && !impl_->start()) {
        delete impl_;
        impl_ = nullptr;
    }
}

Md5::~Md5()
{
    if (impl_) {
        impl_->abort();
        delete impl_;
    }
}

Md5::Md5(Md5&& other) noexcept : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

Md5& Md5::operator=(Md5&& other) noexcept
{
    if (this != &other) {
        if (impl_) {
            impl_->abort();
            delete impl_;
        }
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

bool Md5::update(const uint8_t* data, size_t len)
{
    return impl_ && impl_->update(data, len);
}

bool Md5::finish(uint8_t out[16])
{
    return impl_ && impl_->finish(out);
}

// ── One-shot helpers ──────────────────────────────────────────────────────

bool sha256(const uint8_t* data, size_t len, uint8_t out[32])
{
    if (!data || !out) {
        return false;
    }
    Sha256 ctx;
    return ctx.update(data, len) && ctx.finish(out);
}

bool hmacSha256(const uint8_t* key, size_t keyLen,
                const uint8_t* msg, size_t msgLen, uint8_t out[32])
{
    if (!key || !msg || !out || keyLen == 0) {
        return false;
    }

#if EMBED_CRYPTO_PSA
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(keyLen) * 8);

    psa_key_id_t keyId = 0;
    if (!psaOk(psa_import_key(&attributes, key, keyLen, &keyId), "hmac import key")) {
        return false;
    }

    psa_mac_operation_t op = PSA_MAC_OPERATION_INIT;
    bool ok = psaOk(psa_mac_sign_setup(&op, keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256)),
                    "hmac setup");
    if (ok) {
        ok = psaOk(psa_mac_update(&op, msg, msgLen), "hmac update");
    }
    size_t macLen = 0;
    if (ok) {
        ok = psaOk(psa_mac_sign_finish(&op, out, 32, &macLen), "hmac finish") &&
             macLen == 32;
    } else {
        psa_mac_abort(&op);
    }
    psa_destroy_key(keyId);
    return ok;
#else
    static constexpr size_t kBlock = 64;
    static constexpr size_t kDigest = 32;

    uint8_t kIpad[kBlock]{};
    uint8_t kOpad[kBlock]{};
    const size_t useLen = keyLen > kBlock ? kBlock : keyLen;
    std::memcpy(kIpad, key, useLen);
    std::memcpy(kOpad, key, useLen);
    for (size_t i = 0; i < kBlock; ++i) {
        kIpad[i] ^= 0x36;
        kOpad[i] ^= 0x5c;
    }

    uint8_t inner[kDigest]{};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, kIpad, kBlock);
    mbedtls_sha256_update(&ctx, msg, msgLen);
    mbedtls_sha256_finish(&ctx, inner);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, kOpad, kBlock);
    mbedtls_sha256_update(&ctx, inner, kDigest);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
    return true;
#endif
}

} // namespace embed::crypto
