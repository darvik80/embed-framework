#pragma once

#include "embed/config.hpp"
#include "embed/message.hpp"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"

namespace embed {

/// Wrapper around the ESP-IDF default event loop.
///
/// Uses esp_event_loop_create_default() so the same loop serves
/// WiFi, IP, and framework events — no extra task needed.
/// The default loop manages its own internal synchronisation,
/// so no additional mutex is required.
///
/// Must be initialized before any signals are emitted.
/// Call init() once at application startup.
class EventLoop {
public:
    /// Singleton access.
    static EventLoop& instance();

    /// Create the default event loop (idempotent).
    /// Must be called once before using signals.
    void init();

    /// Delete the default event loop.
    void deinit();

    /// Post a trivially-copyable message to the default event loop.
    /// Waits up to EMBED_EVENT_POST_TIMEOUT_MS (or forever if set to -1).
    /// Returns ESP_OK, ESP_ERR_TIMEOUT if the queue is full, or other esp_err_t.
    template<Message M>
    esp_err_t post(esp_event_base_t base, int32_t id, const M& msg) {
        if (!initialized_) return ESP_ERR_INVALID_STATE;
#if EMBED_EVENT_POST_TIMEOUT_MS < 0
        const TickType_t ticks = portMAX_DELAY;
#else
        const TickType_t ticks = pdMS_TO_TICKS(EMBED_EVENT_POST_TIMEOUT_MS);
#endif
        return esp_event_post(base, id, &msg, sizeof(M), ticks);
    }

    /// Register a handler on the default event loop.
    /// Returns ESP_OK and fills `instance` on success.
    /// This allows multiple registrations of the same handler function
    /// with different arguments — each gets a unique instance handle.
    esp_err_t registerHandler(esp_event_base_t base,
                               int32_t id,
                               esp_event_handler_t handler,
                               void* arg,
                               esp_event_handler_instance_t* instance);

    /// Unregister a handler by its instance handle.
    /// This correctly removes the specific registration even if the
    /// same handler function was registered multiple times.
    esp_err_t unregisterHandler(esp_event_base_t base,
                                 int32_t id,
                                 esp_event_handler_instance_t instance);

    bool initialized() const { return initialized_; }

private:
    EventLoop() = default;
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    bool initialized_ = false;
};

} // namespace embed
