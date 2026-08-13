#include "embed/event_loop.hpp"

namespace embed {

EventLoop& EventLoop::instance() {
    static EventLoop loop;
    return loop;
}

void EventLoop::init() {
    if (initialized_) return;

    // Create the system default event loop (idempotent at ESP-IDF level).
    // WiFi, IP, and framework events all share this single loop.
    esp_err_t err = esp_event_loop_create_default();
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means the default loop already exists —
        // that is fine, we can still use it.
        initialized_ = true;
    }
}

void EventLoop::deinit() {
    if (!initialized_) return;

    esp_event_loop_delete_default();
    initialized_ = false;
}

esp_err_t EventLoop::registerHandler(esp_event_base_t base,
                                      int32_t id,
                                      esp_event_handler_t handler,
                                      void* arg,
                                      esp_event_handler_instance_t* instance) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    return esp_event_handler_instance_register(
        base, id, handler, arg, instance);
}

esp_err_t EventLoop::unregisterHandler(esp_event_base_t base,
                                        int32_t id,
                                        esp_event_handler_instance_t instance) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    return esp_event_handler_instance_unregister(
        base, id, instance);
}

} // namespace embed
