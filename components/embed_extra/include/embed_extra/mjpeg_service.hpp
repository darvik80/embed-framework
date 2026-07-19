#pragma once

#include "embed/embed.hpp"
#include "embed_extra/camera_service.hpp"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace embed {

/// Service that streams MJPEG over HTTP by subscribing to CameraService.
///
/// Registers a single GET endpoint at /stream that serves
/// multipart/x-mixed-replace MJPEG to any connected HTTP client.
///
/// Usage:
///   auto* cam  = registry.createService<CameraService>();
///   auto* mjpeg = registry.createService<MjpegService>();
///   registry.startAll();
///   // Connect browser to http://<ip>/stream
///
/// At most one simultaneous stream client is supported by default
/// (httpd max_open_sockets = 1 for the MJPEG server).
/// Increase CONFIG_EMBED_MJPEG_STACK_SIZE if streaming stalls.
class MjpegService : public Service {
public:
    MjpegService();
    ~MjpegService() override = default;

    const char* serviceName() const override { return "MjpegService"; }

    void start() override;
    void stop() override;

private:
    httpd_handle_t        server_    = nullptr;
    QueueHandle_t         queue_     = nullptr;
    Slot<CameraFrame>     frameSlot_;

    static void      onFrameReceived(const CameraFrame& msg, void* ctx);
    static esp_err_t streamHandler(httpd_req_t* req);
};

} // namespace embed
