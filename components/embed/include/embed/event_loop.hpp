#pragma once

#include "embed/config.hpp"
#include "embed/message.hpp"
#include "esp_event.h"

#if EMBED_THREAD_SAFE
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

namespace embed {

/// Wrapper around esp_event_loop for the framework.
///
/// Creates a dedicated event loop (not the system default one)
/// with configurable task priority, stack size, and queue size.
///
/// Must be initialized before any signals are emitted.
/// Call init() once at application startup.
class EventLoop {
public:
    /// Singleton access.
    static EventLoop& instance();

    /// Create and start the event loop.
    /// Must be called once before using signals.
    void init();

    /// Stop and destroy the event loop.
    void deinit();

    /// Get the underlying esp_event_loop handle.
    esp_event_loop_handle_t handle() const { return handle_; }

    /// Post a trivially-copyable message to the event loop.
    template<Message M>
    void post(esp_event_base_t base, int32_t id, const M& msg) {
        if (!handle_) return;
        esp_event_post_to(handle_, base, id,
                          &msg, sizeof(M), portMAX_DELAY);
    }

    /// Register a handler using the instance-based API.
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

    esp_event_loop_handle_t handle_ = nullptr;
    bool initialized_ = false;

#if EMBED_THREAD_SAFE
    SemaphoreHandle_t mutex_ = nullptr;
#endif
};

} // namespace embed
