#pragma once

#include "embed/message.hpp"
#include "embed/connection.hpp"
#include "embed/signal.hpp"
#include "embed/event_loop.hpp"
#include "esp_event.h"

namespace embed {

/// A slot that receives messages of type M from a Signal<M>.
///
/// Usage:
///   Slot<ButtonPressed> slot([](const ButtonPressed& msg, void* ctx) {
///       // handle message
///   }, this);
///   slot.connect(someSignal);
///
/// The callback signature is: void(const M& msg, void* ctx)
/// where ctx is the user-provided context pointer.
///
/// When the Slot is destroyed, the connection is automatically disconnected.
/// Multiple Slot<M> instances can connect to the same Signal<M> — each
/// gets its own esp_event_handler_instance_t for correct unregistration.
template<Message M>
class Slot {
public:
    /// Callback type: receives the message and a user context pointer.
    using Callback = void(*)(const M& msg, void* ctx);

    /// Construct a slot with a callback and optional context.
    Slot(Callback cb, void* ctx = nullptr)
        : callback_(cb), ctx_(ctx) {}

    /// Destroy the slot — automatically disconnects.
    ~Slot() = default;

    /// Connect this slot to a Signal<M>.
    /// If already connected, disconnects the previous connection first.
    /// Returns a reference to the internal Connection for inspection.
    Connection& connect(Signal<M>& signal) {
        // Disconnect any previous connection
        connection_.disconnect();

        // Register our trampoline using the instance-based API.
        // This returns an instance handle that uniquely identifies
        // this registration, even if the same handler function
        // is registered multiple times.
        esp_event_handler_instance_t instance = nullptr;
        esp_err_t err = EventLoop::instance().registerHandler(
            signal.eventBase(),
            Signal<M>::eventId(),
            &Slot<M>::trampoline,
            this,  // Pass 'this' as handler_arg so trampoline can call back
            &instance
        );

        if (err != ESP_OK || instance == nullptr) {
            return connection_;
        }

        // Store the instance handle in the connection pool.
        // If the pool is exhausted, unregister immediately — otherwise the
        // handler stays registered with no Connection to clean it up.
        connection_ = ConnectionPool::instance().allocate(
            signal.eventBase(),
            Signal<M>::eventId(),
            instance
        );
        if (!connection_.connected()) {
            EventLoop::instance().unregisterHandler(
                signal.eventBase(),
                Signal<M>::eventId(),
                instance
            );
        }

        return connection_;
    }

    /// Disconnect this slot from any signal.
    void disconnect() {
        connection_.disconnect();
    }

    /// Check if this slot is currently connected.
    bool isConnected() const {
        return connection_.connected();
    }

    // Non-copyable, non-movable (holds connection with pool index)
    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
    Slot(Slot&&) = delete;
    Slot& operator=(Slot&&) = delete;

private:
    Callback callback_;
    void* ctx_;
    Connection connection_;

    /// C -> C++ trampoline: called by esp_event_loop,
    /// reinterprets event_data as const M* and invokes the callback.
    static void trampoline(void* handler_arg,
                           esp_event_base_t /*base*/,
                           int32_t /*id*/,
                           void* event_data) {
        if (!event_data) return;
        auto* self = static_cast<Slot<M>*>(handler_arg);
        const M& msg = *static_cast<const M*>(event_data);
        self->callback_(msg, self->ctx_);
    }
};

} // namespace embed
