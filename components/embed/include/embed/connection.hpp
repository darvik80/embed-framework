#pragma once

#include "embed/config.hpp"
#include "esp_event.h"
#include <array>

namespace embed {

/// RAII handle for a signal-slot connection.
///
/// When a Connection is destroyed, it automatically disconnects
/// from the event loop and frees the connection pool slot.
///
/// Connections are move-only (not copyable).
class Connection {
public:
    Connection() = default;
    ~Connection();

    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    /// Returns true if this connection is active.
    bool connected() const { return index_ >= 0; }

    /// Disconnect from the event loop and release the pool slot.
    void disconnect();

private:
    friend class ConnectionPool;

    /// Internal: create a connection tied to pool index.
    explicit Connection(int index) : index_(index) {}

    int index_ = -1;  // Index into the static connection pool; -1 = not connected
};

/// Static pool that tracks all active signal-slot connections.
/// Uses esp_event_handler_instance_t for correct unregistration
/// when multiple slots of the same type are registered.
class ConnectionPool {
public:
    /// Singleton access.
    static ConnectionPool& instance();

    /// Allocate a pool slot for a new connection.
    /// The instance handle identifies this specific registration
    /// and is used for correct unregistration.
    /// Returns a Connection tied to the allocated slot,
    /// or a disconnected Connection if the pool is exhausted.
    Connection allocate(esp_event_base_t base,
                        int32_t id,
                        esp_event_handler_instance_t instance);

    /// Release a pool slot and unregister from the event loop.
    void release(int index);

    /// Get the number of active connections.
    size_t count() const;

private:
    ConnectionPool() = default;

    struct Entry {
        esp_event_base_t base = nullptr;
        int32_t id = 0;
        esp_event_handler_instance_t instance = nullptr;
        bool active = false;
    };

    std::array<Entry, EMBED_MAX_CONNECTIONS> entries_{};
};

} // namespace embed
