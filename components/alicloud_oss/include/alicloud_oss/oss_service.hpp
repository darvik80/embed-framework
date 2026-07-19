#pragma once

#include "embed/embed.hpp"
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif
#include "alicloud_oss_client.h"
#include "alicloud_oss_default.h"
#ifdef __cplusplus
}
#endif

namespace embed {

/// Emitted when a putObject() operation completes (success or failure).
struct OssUploadComplete {
    bool     success;       ///< true = HTTP 200 received
    uint16_t httpStatus;    ///< Raw HTTP status code
    uint32_t bytesUploaded; ///< Bytes written (0 on failure)
    uint32_t requestId;     ///< User-supplied correlation ID
};
static_assert(embed::Message<OssUploadComplete>);

/// Emitted when a deleteObject() operation completes.
struct OssDeleteComplete {
    bool     success;
    uint16_t httpStatus;
    uint32_t requestId;
};
static_assert(embed::Message<OssDeleteComplete>);

/// Service that wraps the Alibaba Cloud OSS C client.
///
/// Credentials are read from Kconfig (CONFIG_EMBED_OSS_*) at start().
///
/// All methods are **synchronous and blocking** — call them from a
/// FreeRTOS task, never from a Slot callback (EventLoop task context).
///
/// Two upload styles:
///
///   // 1. Buffer upload (small objects already in memory):
///   oss->putObject("cfg/settings.json", buf, len);
///
///   // 2. Streaming upload via producer callback (large files, camera frames):
///   oss->putObjectStream("images/frame.jpg", totalSize,
///       [](void* dst, size_t want, size_t* got) -> esp_err_t {
///           *got = fread(dst, 1, want, fp);
///           return (*got > 0 || feof(fp)) ? ESP_OK : ESP_FAIL;
///       });
///
///   // 3. Streaming download via consumer callback:
///   oss->getObjectStream("images/frame.jpg",
///       [](const void* chunk, size_t len) -> esp_err_t {
///           fwrite(chunk, 1, len, fp);
///           return ESP_OK;
///       });
///
///   // 4. Small-object download into a caller-supplied buffer:
///   oss->getObject("cfg/settings.json", buf, sizeof(buf), &outLen);
class OssService : public Service {
public:
    /// Producer: fill up to `want` bytes into `dst`, set `*got` to actual count.
    /// Return ESP_OK to continue, any error to abort.
    /// When the source is exhausted, set `*got = 0` and return ESP_OK.
    using Producer = std::function<esp_err_t(void* dst, size_t want, size_t* got)>;

    /// Consumer: process one incoming chunk of `len` bytes from `data`.
    /// Return ESP_OK to continue, any error to abort.
    using Consumer = std::function<esp_err_t(const void* data, size_t len)>;

    OssService() = default;
    ~OssService() override = default;

    const char* serviceName() const override { return "OssService"; }

    void start() override;
    void stop() override;

    // ── Upload ────────────────────────────────────────────────────────

    /// Upload a buffer already in memory (convenience wrapper over putObjectStream).
    /// Emits onUploadComplete when done (even on failure).
    esp_err_t putObject(const char* key, const void* data, size_t len,
                        const char* contentType = "application/octet-stream",
                        uint32_t requestId = 0);

    /// Streaming upload: total size must be known up front (sets Content-Length).
    /// `producer` is called repeatedly with chunks of `chunkSize` bytes until
    /// it returns *got == 0 or an error.
    /// Emits onUploadComplete when done (even on failure).
    esp_err_t putObjectStream(const char* key, size_t totalSize, Producer producer,
                              const char* contentType = "application/octet-stream",
                              size_t chunkSize = 4096,
                              uint32_t requestId = 0);

    // ── Download ──────────────────────────────────────────────────────

    /// Download into a caller-supplied buffer (suitable for small objects).
    /// outLen receives actual bytes read.
    esp_err_t getObject(const char* key, void* buf, size_t bufLen, size_t* outLen);

    /// Streaming download: `consumer` is called for each received chunk.
    /// `chunkBufSize` is the size of the internal receive buffer (stack-allocated).
    esp_err_t getObjectStream(const char* key, Consumer consumer,
                               size_t chunkBufSize = 4096);

    // ── Other operations ──────────────────────────────────────────────

    /// Delete an OSS object. Emits onDeleteComplete when done.
    esp_err_t deleteObject(const char* key, uint32_t requestId = 0);

    /// Retrieve object metadata without downloading content.
    /// Caller must call alicloud_oss_free_object_metadata(meta) when done.
    esp_err_t headObject(const char* key, alicloud_oss_object_metadata_t* meta);

    // ── Signals ───────────────────────────────────────────────────────

    Signal<OssUploadComplete> onUploadComplete;
    Signal<OssDeleteComplete> onDeleteComplete;

private:
    alicloud_oss_handler_t client_ = nullptr;

    /// Default chunk size used internally when streaming (heap-allocated).
    static constexpr size_t kDefaultChunkSize = 4096;
};

} // namespace embed
