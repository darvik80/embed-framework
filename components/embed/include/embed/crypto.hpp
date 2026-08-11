#pragma once

#include <cstddef>
#include <cstdint>

namespace embed::crypto {

/// Incremental SHA-256 (OTA image verify, etc.).
class Sha256 {
public:
    Sha256();
    ~Sha256();

    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    Sha256(Sha256&& other) noexcept;
    Sha256& operator=(Sha256&& other) noexcept;

    bool update(const uint8_t* data, size_t len);
    bool finish(uint8_t out[32]);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

/// Incremental MD5 (Alibaba OTA digest).
class Md5 {
public:
    Md5();
    ~Md5();

    Md5(const Md5&) = delete;
    Md5& operator=(const Md5&) = delete;
    Md5(Md5&& other) noexcept;
    Md5& operator=(Md5&& other) noexcept;

    bool update(const uint8_t* data, size_t len);
    bool finish(uint8_t out[16]);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

/// One-shot SHA-256.
bool sha256(const uint8_t* data, size_t len, uint8_t out[32]);

/// HMAC-SHA256 (`key` may be up to 64 bytes; longer keys are hashed first per RFC 2104).
bool hmacSha256(const uint8_t* key, size_t keyLen,
                const uint8_t* msg, size_t msgLen, uint8_t out[32]);

} // namespace embed::crypto
