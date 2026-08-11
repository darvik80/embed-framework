#pragma once

#include "embed/embed.hpp"
#include "embed_extra/camera_service.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace embed {

/// Uploads camera frames to Alibaba Cloud OSS via `OssService`.
///
/// Subscribes to `CameraService::onFrame` and uploads each frame as a raw
/// buffer. The OSS key format is: `frames/<timestamp_ms>_<seq>.jpg`
///
/// Usage:
///   auto* cam    = registry.createService<CameraService>();
///   auto* oss    = registry.createService<OssService>();
///   auto* upload = registry.createService<OssUploadService>();
///   registry.startAll();
class OssUploadService : public Service {
public:
    OssUploadService();
    ~OssUploadService() override = default;

    const char* serviceName() const override { return "OssUploadService"; }

    void start() override;
    void stop() override;

private:
    QueueHandle_t     queue_      = nullptr;
    TaskHandle_t      uploadTask_ = nullptr;
    Slot<CameraFrame> frameSlot_;

    static void onFrameReceived(const CameraFrame& msg, void* ctx);
    static void uploadTaskFunc(void* arg);
};

} // namespace embed
