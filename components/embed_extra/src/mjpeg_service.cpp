#include "embed_extra/mjpeg_service.hpp"
#include "embed/embed.hpp"
#include "esp_camera.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "MjpegService";

#define MJPEG_BOUNDARY     "EMBEDFRAME"
#define MJPEG_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" MJPEG_BOUNDARY
#define MJPEG_PART_HDR     "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n"
#define MJPEG_BOUNDARY_STR "\r\n--" MJPEG_BOUNDARY "\r\n"

namespace embed {

// ── Constructor ───────────────────────────────────────────────────────────

MjpegService::MjpegService()
    : frameSlot_(&MjpegService::onFrameReceived, this)
{}

// ── Lifecycle ─────────────────────────────────────────────────────────────

void MjpegService::start() {
    queue_ = xQueueCreate(CONFIG_EMBED_MJPEG_QUEUE_DEPTH, sizeof(CameraFrame));
    if (!queue_) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return;
    }

    // Connect to CameraService
    auto* cam = ServiceRegistry::instance().getService<CameraService>();
    if (!cam) {
        ESP_LOGE(TAG, "CameraService not found in registry");
        return;
    }
    frameSlot_.connect(cam->onFrame);

    // Start HTTP server
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = CONFIG_EMBED_MJPEG_PORT;
    cfg.stack_size       = CONFIG_EMBED_MJPEG_STACK_SIZE;
    cfg.max_open_sockets = 4;
    cfg.uri_match_fn     = httpd_uri_match_wildcard;
    cfg.core_id = 1;

    if (httpd_start(&server_, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server on port %d", CONFIG_EMBED_MJPEG_PORT);
        return;
    }

    httpd_uri_t stream_uri = {
        .uri      = "/stream",
        .method   = HTTP_GET,
        .handler  = streamHandler,
        .user_ctx = this,
    };
    httpd_register_uri_handler(server_, &stream_uri);

    ESP_LOGI(TAG, "MJPEG server started on port %d — http://<ip>/stream",
             CONFIG_EMBED_MJPEG_PORT);
}

void MjpegService::stop() {
    frameSlot_.disconnect();

    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }

    if (queue_) {
        // Drain and return any pending frames
        CameraFrame frame;
        while (xQueueReceive(queue_, &frame, 0) == pdTRUE) {
            if (frame.fb) esp_camera_fb_return(frame.fb);
            else free(frame.data);
        }
        vQueueDelete(queue_);
        queue_ = nullptr;
    }

    ESP_LOGI(TAG, "MJPEG server stopped");
}

// ── Slot callback (EventLoop task context) ────────────────────────────────

void MjpegService::onFrameReceived(const CameraFrame& msg, void* ctx) {
    auto* self = static_cast<MjpegService*>(ctx);
    if (!self->queue_) {
        if (msg.fb) esp_camera_fb_return(msg.fb);
        else free(msg.data);
        return;
    }
    // Single consumer — pass buffer directly to queue without copying
    if (xQueueSend(self->queue_, &msg, 0) != pdTRUE) {
        if (msg.fb) esp_camera_fb_return(msg.fb);
        else free(msg.data);
        ESP_LOGD(TAG, "Frame queue full — dropped seq=%lu", (unsigned long)msg.seq);
    }
}

// ── HTTP stream handler (httpd task context) ──────────────────────────────

esp_err_t MjpegService::streamHandler(httpd_req_t* req) {
    auto* self = static_cast<MjpegService*>(req->user_ctx);

    esp_err_t res = httpd_resp_set_type(req, MJPEG_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");

    char part_hdr[64];

    while (true) {
        if (!self->queue_) break;

        CameraFrame frame;
        if (xQueueReceive(self->queue_, &frame, pdMS_TO_TICKS(2000)) != pdTRUE) {
            // Timeout — client likely still connected; send keep-alive boundary
            res = httpd_resp_send_chunk(req, MJPEG_BOUNDARY_STR,
                                        strlen(MJPEG_BOUNDARY_STR));
            if (res != ESP_OK) break;
            continue;
        }

        // Boundary
        res = httpd_resp_send_chunk(req, MJPEG_BOUNDARY_STR,
                                    strlen(MJPEG_BOUNDARY_STR));
        if (res != ESP_OK) {
            if (frame.fb) esp_camera_fb_return(frame.fb);
            else free(frame.data);
            break;
        }

        // Part header with content length
        int hlen = snprintf(part_hdr, sizeof(part_hdr), MJPEG_PART_HDR, frame.len);
        res = httpd_resp_send_chunk(req, part_hdr, (size_t)hlen);
        if (res != ESP_OK) {
            if (frame.fb) esp_camera_fb_return(frame.fb);
            else free(frame.data);
            break;
        }

        // JPEG payload
        res = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(frame.data),
                                    frame.len);
        if (frame.fb) esp_camera_fb_return(frame.fb);
        else free(frame.data);

        if (res != ESP_OK) break;
    }

    // Terminate chunked response
    httpd_resp_send_chunk(req, nullptr, 0);
    ESP_LOGI(TAG, "Client disconnected");
    return res;
}

} // namespace embed
