#include "embed_extra/camera_service.hpp"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "CameraService";

namespace embed {

// ── Pin configuration ─────────────────────────────────────────────────────

void CameraService::fillCameraConfig(void* cfg_ptr) {
    auto& cfg = *static_cast<camera_config_t*>(cfg_ptr);

#if defined(CONFIG_EMBED_CAMERA_BOARD_FREENOVE_S3)
    cfg.pin_pwdn     = -1;
    cfg.pin_reset    = -1;
    cfg.pin_xclk     = 15;
    cfg.pin_sccb_sda = 4;
    cfg.pin_sccb_scl = 5;
    cfg.pin_d7       = 16;
    cfg.pin_d6       = 17;
    cfg.pin_d5       = 18;
    cfg.pin_d4       = 12;
    cfg.pin_d3       = 10;
    cfg.pin_d2       = 8;
    cfg.pin_d1       = 9;
    cfg.pin_d0       = 11;
    cfg.pin_vsync    = 6;
    cfg.pin_href     = 7;
    cfg.pin_pclk     = 13;
#else // AI-Thinker ESP32-CAM
    cfg.pin_pwdn     = 32;
    cfg.pin_reset    = -1;
    cfg.pin_xclk     = 0;
    cfg.pin_sccb_sda = 26;
    cfg.pin_sccb_scl = 27;
    cfg.pin_d7       = 35;
    cfg.pin_d6       = 34;
    cfg.pin_d5       = 39;
    cfg.pin_d4       = 36;
    cfg.pin_d3       = 21;
    cfg.pin_d2       = 19;
    cfg.pin_d1       = 18;
    cfg.pin_d0       = 5;
    cfg.pin_vsync    = 25;
    cfg.pin_href     = 23;
    cfg.pin_pclk     = 22;
#endif

    cfg.xclk_freq_hz  = 26000000;
    cfg.ledc_timer    = LEDC_TIMER_0;
    cfg.ledc_channel  = LEDC_CHANNEL_0;
    cfg.pixel_format  = PIXFORMAT_JPEG;
    cfg.frame_size    = FRAMESIZE_5MP;
    cfg.jpeg_quality  = 8;
    cfg.fb_count      = 2;
    cfg.fb_location   = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode     = CAMERA_GRAB_LATEST;
    cfg.sccb_i2c_port = -1;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────

void CameraService::start() {
    camera_config_t cfg = {};
    fillCameraConfig(&cfg);

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Camera initialized");

    running_ = true;
    xTaskCreate(captureTaskFunc, "cam_capture",
                4096, this, 5, &captureTask_);
}

void CameraService::stop() {
    running_ = false;
    if (captureTask_) {
        // Give the task time to notice running_ == false
        vTaskDelay(pdMS_TO_TICKS(200));
        captureTask_ = nullptr;
    }
    esp_camera_deinit();
    ESP_LOGI(TAG, "Camera stopped");
}

// ── Capture task ──────────────────────────────────────────────────────────

void CameraService::captureTaskFunc(void* arg) {
    auto* self = static_cast<CameraService*>(arg);

    while (self->running_) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Zero-copy: pass PSRAM buffer directly (CAMERA_FB_IN_PSRAM)
        CameraFrame msg{fb->buf, fb->len, self->seq_++, fb};
        ESP_LOGI(TAG, "Frame seq=%lu len=%zu", (unsigned long)msg.seq, msg.len);
        self->onFrame.emit(msg);

        vTaskDelay(pdMS_TO_TICKS(CONFIG_EMBED_CAMERA_CAPTURE_INTERVAL_MS));
    }

    vTaskDelete(nullptr);
}

} // namespace embed
