#include "alicloud_oss/oss_service.hpp"
#include "esp_log.h"
#include <cstring>
#include <cstdlib>

static const char* TAG = "OssService";

namespace embed {

// ── Lifecycle ─────────────────────────────────────────────────────────────

void OssService::start() {
    alicloud_oss_config_t cfg{};
    strncpy(cfg.access_key_id,     CONFIG_EMBED_OSS_ACCESS_KEY_ID,     sizeof(cfg.access_key_id) - 1);
    strncpy(cfg.access_key_secret, CONFIG_EMBED_OSS_ACCESS_KEY_SECRET, sizeof(cfg.access_key_secret) - 1);
    strncpy(cfg.region,            CONFIG_EMBED_OSS_REGION,            sizeof(cfg.region) - 1);
    strncpy(cfg.bucket,            CONFIG_EMBED_OSS_BUCKET,            sizeof(cfg.bucket) - 1);

    esp_err_t err = alicloud_oss_create(&cfg, &client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create OSS client: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Initialized — region=%s bucket=%s",
             CONFIG_EMBED_OSS_REGION, CONFIG_EMBED_OSS_BUCKET);
}

void OssService::stop() {
    if (client_) {
        alicloud_oss_destroy(client_);
        client_ = nullptr;
    }
    ESP_LOGI(TAG, "Stopped");
}

// ── putObject (buffer) ────────────────────────────────────────────────────

esp_err_t OssService::putObject(const char* key, const void* data, size_t len,
                                const char* contentType, uint32_t requestId) {
    // Wrap buffer as a one-shot producer and delegate to putObjectStream.
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    size_t remaining = len;

    auto producer = [&](void* dst, size_t want, size_t* got) -> esp_err_t {
        *got = (want < remaining) ? want : remaining;
        if (*got > 0) {
            memcpy(dst, ptr, *got);
            ptr       += *got;
            remaining -= *got;
        }
        return ESP_OK;
    };

    return putObjectStream(key, len, producer, contentType, kDefaultChunkSize, requestId);
}

// ── putObjectStream ───────────────────────────────────────────────────────

esp_err_t OssService::putObjectStream(const char* key, size_t totalSize,
                                      Producer producer,
                                      const char* contentType,
                                      size_t chunkSize,
                                      uint32_t requestId) {
    if (!client_) return ESP_ERR_INVALID_STATE;

    if (chunkSize == 0) chunkSize = kDefaultChunkSize;

    alicloud_oss_writer_handler_t writer = nullptr;
    esp_err_t err = alicloud_oss_writer_create(client_, &writer);
    if (err != ESP_OK || !writer) {
        ESP_LOGE(TAG, "putObjectStream: writer_create failed");
        OssUploadComplete msg{false, 0, 0, requestId};
        onUploadComplete.emit(msg);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    err = alicloud_oss_writer_open(writer, key, totalSize);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "putObjectStream: writer_open failed: %s", esp_err_to_name(err));
        alicloud_oss_writer_close(writer);
        OssUploadComplete msg{false, 0, 0, requestId};
        onUploadComplete.emit(msg);
        return err;
    }

    // Stream data via producer callback using a stack-allocated chunk buffer.
    uint8_t* chunkBuf = static_cast<uint8_t*>(malloc(chunkSize));
    if (!chunkBuf) {
        alicloud_oss_writer_close(writer);
        OssUploadComplete msg{false, 0, 0, requestId};
        onUploadComplete.emit(msg);
        return ESP_ERR_NO_MEM;
    }

    size_t totalSent = 0;
    while (err == ESP_OK) {
        size_t got = 0;
        err = producer(chunkBuf, chunkSize, &got);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "putObjectStream: producer error: %s", esp_err_to_name(err));
            break;
        }
        if (got == 0) break; // producer exhausted

        err = alicloud_oss_writer_write(writer, chunkBuf, got);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "putObjectStream: write error: %s", esp_err_to_name(err));
            break;
        }
        totalSent += got;
    }

    free(chunkBuf);
    alicloud_oss_writer_close(writer);

    const bool ok = (err == ESP_OK);
    ESP_LOGI(TAG, "putObjectStream '%s' %zu B — %s", key, totalSent, ok ? "OK" : "FAIL");
    OssUploadComplete msg{ok, ok ? uint16_t{200} : uint16_t{0},
                          static_cast<uint32_t>(totalSent), requestId};
    onUploadComplete.emit(msg);
    return err;
}

// ── getObject (buffer) ────────────────────────────────────────────────────

esp_err_t OssService::getObject(const char* key, void* buf, size_t bufLen, size_t* outLen) {
    // Wrap caller buffer as a consumer and delegate to getObjectStream.
    uint8_t* dst = static_cast<uint8_t*>(buf);
    size_t   remaining = bufLen;
    size_t   total     = 0;

    auto consumer = [&](const void* chunk, size_t len) -> esp_err_t {
        if (len > remaining) len = remaining;
        memcpy(dst, chunk, len);
        dst       += len;
        remaining -= len;
        total     += len;
        return ESP_OK;
    };

    esp_err_t err = getObjectStream(key, consumer, kDefaultChunkSize);
    if (outLen) *outLen = total;
    return err;
}

// ── getObjectStream ───────────────────────────────────────────────────────

esp_err_t OssService::getObjectStream(const char* key, Consumer consumer,
                                       size_t chunkBufSize) {
    if (!client_) return ESP_ERR_INVALID_STATE;

    if (chunkBufSize == 0) chunkBufSize = kDefaultChunkSize;

    alicloud_oss_read_handler_t reader = nullptr;
    esp_err_t err = alicloud_oss_reader_create(client_, &reader);
    if (err != ESP_OK || !reader) {
        ESP_LOGE(TAG, "getObjectStream: reader_create failed");
        return err != ESP_OK ? err : ESP_FAIL;
    }

    err = alicloud_oss_reader_open(reader, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "getObjectStream: reader_open failed: %s", esp_err_to_name(err));
        alicloud_oss_reader_close(reader);
        return err;
    }

    uint8_t* chunkBuf = static_cast<uint8_t*>(malloc(chunkBufSize));
    if (!chunkBuf) {
        alicloud_oss_reader_close(reader);
        return ESP_ERR_NO_MEM;
    }

    size_t total = 0;
    while (err == ESP_OK) {
        int readLen = 0;
        err = alicloud_oss_reader_read(reader, chunkBuf,
                                       static_cast<int>(chunkBufSize), &readLen);
        if (err != ESP_OK || readLen == 0) break;

        esp_err_t consErr = consumer(chunkBuf, static_cast<size_t>(readLen));
        if (consErr != ESP_OK) {
            ESP_LOGE(TAG, "getObjectStream: consumer error: %s", esp_err_to_name(consErr));
            err = consErr;
            break;
        }
        total += static_cast<size_t>(readLen);
    }

    free(chunkBuf);
    alicloud_oss_reader_close(reader);

    // reader_read returning ESP_OK with readLen==0 means EOF — that's success.
    if (err == ESP_OK || (err == ESP_OK && total > 0)) err = ESP_OK;
    ESP_LOGI(TAG, "getObjectStream '%s' read %zu B — %s",
             key, total, err == ESP_OK ? "OK" : "FAIL");
    return err;
}

// ── deleteObject ──────────────────────────────────────────────────────────

esp_err_t OssService::deleteObject(const char* key, uint32_t requestId) {
    if (!client_) return ESP_ERR_INVALID_STATE;

    esp_err_t err = alicloud_oss_delete_object(client_, key);
    const bool ok = (err == ESP_OK);
    ESP_LOGI(TAG, "deleteObject '%s' — %s", key, ok ? "OK" : "FAIL");
    OssDeleteComplete msg{ok, ok ? uint16_t{204} : uint16_t{0}, requestId};
    onDeleteComplete.emit(msg);
    return err;
}

// ── headObject ────────────────────────────────────────────────────────────

esp_err_t OssService::headObject(const char* key, alicloud_oss_object_metadata_t* meta) {
    if (!client_) return ESP_ERR_INVALID_STATE;
    return alicloud_oss_head_object(client_, key, meta);
}

} // namespace embed
