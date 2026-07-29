#pragma once

#include "embed/embed.hpp"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include <cstddef>
#include <cstdlib>

namespace embed {

/// A camera frame distributed via Signal.
///
/// When fb is non-null, the frame uses the camera driver's PSRAM buffer
/// directly (zero-copy). The consumer MUST call esp_camera_fb_return(fb)
/// after processing to release the buffer back to the camera driver.
///
/// When fb is null, data is a standalone heap buffer that must be free()'d.
struct CameraFrame {
    uint8_t*     data;  ///< Frame payload pointer
    size_t       len;   ///< Frame byte length
    uint32_t     seq;   ///< Monotonically increasing frame sequence number
    camera_fb_t* fb;    ///< Camera FB handle for zero-copy return (may be null)
};
static_assert(embed::Message<CameraFrame>);

/// Release a frame buffer according to ownership rules.
/// Prefer esp_camera_fb_return when fb is set; otherwise free(data).
inline void releaseCameraFrame(CameraFrame& frame) {
    if (frame.fb) {
        esp_camera_fb_return(frame.fb);
        frame.fb = nullptr;
        frame.data = nullptr;
    } else if (frame.data) {
        free(frame.data);
        frame.data = nullptr;
    }
    frame.len = 0;
}

inline void releaseCameraFrame(const CameraFrame& frame) {
    CameraFrame copy = frame;
    releaseCameraFrame(copy);
}

/// Service that captures JPEG frames from the ESP32 camera driver
/// and distributes them to subscribers via Signal<CameraFrame>.
///
/// Usage:
///   auto* cam = registry.createService<CameraService>();
///   // In another service's start():
///   slot_.connect(cam->onFrame);
///
/// The camera is initialized using Kconfig pin settings (EMBED_CAMERA_BOARD_*).
/// Frames are captured at CONFIG_EMBED_CAMERA_CAPTURE_INTERVAL_MS intervals.
class CameraService : public Service {
public:
    CameraService() = default;
    ~CameraService() override = default;

    const char* serviceName() const override { return "CameraService"; }

    void start() override;
    void stop() override;

    /// Emitted for each captured frame.
    /// Subscriber owns the frame buffer and must free() it.
    Signal<CameraFrame> onFrame;

private:
    TaskHandle_t  captureTask_ = nullptr;
    uint32_t      seq_         = 0;
    volatile bool running_     = false;

    static void captureTaskFunc(void* arg);
    static void fillCameraConfig(void* cfg_ptr);
};

} // namespace embed
