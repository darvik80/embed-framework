#include "embed/event_loop.hpp"

namespace embed {

EventLoop& EventLoop::instance() {
    static EventLoop loop;
    return loop;
}

void EventLoop::init() {
    if (initialized_) return;

#if EMBED_THREAD_SAFE
    mutex_ = xSemaphoreCreateMutex();
#endif

    // ESP-IDF v5.5 struct order: queue_size, task_name, task_priority, task_stack_size, task_core_id
    esp_event_loop_args_t args = {
        .queue_size = EMBED_EVENT_QUEUE_SIZE,
        .task_name = "embed_evt",
        .task_priority = EMBED_EVENT_TASK_PRIORITY,
        .task_stack_size = EMBED_EVENT_TASK_STACK_SIZE,
        .task_core_id = tskNO_AFFINITY,
    };

    esp_err_t err = esp_event_loop_create(&args, &handle_);
    if (err == ESP_OK) {
        initialized_ = true;
    }
}

void EventLoop::deinit() {
    if (!initialized_) return;

    if (handle_) {
        esp_event_loop_delete(handle_);
        handle_ = nullptr;
    }
    initialized_ = false;

#if EMBED_THREAD_SAFE
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
#endif
}

esp_err_t EventLoop::registerHandler(esp_event_base_t base,
                                      int32_t id,
                                      esp_event_handler_t handler,
                                      void* arg,
                                      esp_event_handler_instance_t* instance) {
    if (!handle_) return ESP_ERR_INVALID_STATE;
    return esp_event_handler_instance_register_with(
        handle_, base, id, handler, arg, instance);
}

esp_err_t EventLoop::unregisterHandler(esp_event_base_t base,
                                        int32_t id,
                                        esp_event_handler_instance_t instance) {
    if (!handle_) return ESP_ERR_INVALID_STATE;
    return esp_event_handler_instance_unregister_with(
        handle_, base, id, instance);
}

} // namespace embed
