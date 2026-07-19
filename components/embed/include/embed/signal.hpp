#pragma once

#include "embed/message.hpp"
#include "embed/event_loop.hpp"
#include "esp_event.h"
#include <typeinfo>

namespace embed {

/// Holds a unique esp_event_base_t for each message type M.
/// The base string is derived from typeid(M).name() (available since RTTI is enabled).
/// Lazily initialized on first access.
template<Message M>
struct EventBaseHolder {
    static const char* get() {
        static const char* base_str = nullptr;
        if (!base_str) {
            // Use a static buffer to hold the event base string.
            // typeid().name() returns a stable pointer for the lifetime of the program.
            base_str = typeid(M).name();
        }
        return base_str;
    }

    static esp_event_base_t eventBase() {
        return get();
    }
};

/// A signal that can be emitted to send messages of type M
/// through the event loop.
///
/// Each Signal<M> type automatically gets a unique event_base
/// derived from the message type's typeid.
///
/// Usage:
///   Signal<ButtonPressed> onButton;
///   onButton.emit({.id = 1, .pressed = true});
template<Message M>
class Signal {
public:
    Signal() : base_(EventBaseHolder<M>::eventBase()) {}

    /// Emit a message through the event loop.
    /// The message is copied into the event queue.
    void emit(const M& msg) {
        EventLoop::instance().post<M>(base_, kEventId, msg);
    }

    /// Get this signal type's event base (needed for slot connection).
    esp_event_base_t eventBase() const { return base_; }

    /// Event ID used by this signal type (always 0 — type is distinguished by base).
    static constexpr int32_t eventId() { return kEventId; }

private:
    static constexpr int32_t kEventId = 0;
    esp_event_base_t base_;
};

} // namespace embed
