#include "alicloud_oss/oss_upload_service.hpp"
#include "alicloud_oss/oss_service.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include <cstdio>
#include <cstring>
#include <sys/time.h>

static const char* TAG = "OssUploadService";

#if defined(CONFIG_EMBED_OSS_UPLOAD_QUEUE_DEPTH)
static constexpr UBaseType_t kQueueDepth = CONFIG_EMBED_OSS_UPLOAD_QUEUE_DEPTH;
#else
static constexpr UBaseType_t kQueueDepth = 2;
#endif

namespace embed {

OssUploadService::OssUploadService()
    : frameSlot_(&OssUploadService::onFrameReceived, this)
{}

void OssUploadService::start()
{
    queue_ = xQueueCreate(kQueueDepth, sizeof(CameraFrame));
    if (!queue_) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return;
    }

    auto* cam = ServiceRegistry::instance().getService<CameraService>();
    if (!cam) {
        ESP_LOGE(TAG, "CameraService not found in registry");
        return;
    }
    frameSlot_.connect(cam->onFrame);

    xTaskCreatePinnedToCore(uploadTaskFunc, "oss_upload", 8192, this, 5, &uploadTask_, 1);

    ESP_LOGI(TAG, "Started — uploading frames to OSS");
}

void OssUploadService::stop()
{
    frameSlot_.disconnect();

    if (uploadTask_) {
        vTaskDelete(uploadTask_);
        uploadTask_ = nullptr;
    }

    if (queue_) {
        CameraFrame frame;
        while (xQueueReceive(queue_, &frame, 0) == pdTRUE) {
            // Queued frames are heap copies (fb == nullptr).
            releaseCameraFrame(frame);
        }
        vQueueDelete(queue_);
        queue_ = nullptr;
    }

    ESP_LOGI(TAG, "Stopped");
}

void OssUploadService::onFrameReceived(const CameraFrame& msg, void* ctx)
{
    auto* self = static_cast<OssUploadService*>(ctx);
    if (!self->queue_) {
        releaseCameraFrame(msg);
        return;
    }

    // Copy payload into SPIRAM for the upload task, then release the camera FB
    // (or heap buffer) owned by the Signal message.
    auto* copy = static_cast<uint8_t*>(
        heap_caps_malloc(msg.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!copy) {
        releaseCameraFrame(msg);
        ESP_LOGW(TAG, "SPIRAM alloc failed for frame copy — dropping");
        return;
    }
    memcpy(copy, msg.data, msg.len);
    releaseCameraFrame(msg);

    CameraFrame queued{copy, msg.len, msg.seq, nullptr};
    if (xQueueSend(self->queue_, &queued, 0) != pdTRUE) {
        free(copy);
        ESP_LOGD(TAG, "Frame queue full — dropped seq=%lu", (unsigned long)msg.seq);
    } else {
        ESP_LOGI(TAG, "Frame queued seq=%lu", (unsigned long)msg.seq);
    }
}

void OssUploadService::uploadTaskFunc(void* arg)
{
    auto* self = static_cast<OssUploadService*>(arg);

    auto* oss = ServiceRegistry::instance().getService<OssService>();
    if (!oss) {
        ESP_LOGE(TAG, "OssService not found in registry");
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        CameraFrame frame;
        if (xQueueReceive(self->queue_, &frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        struct timeval tv;
        gettimeofday(&tv, nullptr);
        const int64_t timestampMs =
            static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;

        char key[128];
        snprintf(key, sizeof(key), "frames/%lld_%lu.jpg",
                 static_cast<long long>(timestampMs),
                 static_cast<unsigned long>(frame.seq));

        const esp_err_t err = oss->putObject(key, frame.data, frame.len,
                                             "application/octet-stream", frame.seq);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Uploaded %s (%zu B)", key, frame.len);
        } else {
            ESP_LOGW(TAG, "Upload failed for %s: %s", key, esp_err_to_name(err));
        }

        releaseCameraFrame(frame);
    }
}

} // namespace embed
