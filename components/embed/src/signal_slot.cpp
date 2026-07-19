#include "embed/connection.hpp"
#include "embed/event_loop.hpp"

namespace embed {

// ── ConnectionPool ──────────────────────────────────────────────────────

ConnectionPool& ConnectionPool::instance() {
    static ConnectionPool pool;
    return pool;
}

Connection ConnectionPool::allocate(esp_event_base_t base,
                                    int32_t id,
                                    esp_event_handler_instance_t instance) {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (!entries_[i].active) {
            entries_[i] = {base, id, instance, true};
            return Connection{i};
        }
    }
    // Pool exhausted — return disconnected connection
    return Connection{};
}

void ConnectionPool::release(int index) {
    if (index < 0 || index >= static_cast<int>(entries_.size())) return;
    if (!entries_[index].active) return;

    auto& entry = entries_[index];

    // Unregister using the instance handle — this correctly removes
    // the specific registration even if the same handler function
    // was registered multiple times (e.g., multiple Slot<M> instances).
    if (entry.instance) {
        EventLoop::instance().unregisterHandler(entry.base, entry.id, entry.instance);
    }

    entry.active = false;
    entry.base = nullptr;
    entry.id = 0;
    entry.instance = nullptr;
}

size_t ConnectionPool::count() const {
    size_t n = 0;
    for (const auto& entry : entries_) {
        if (entry.active) ++n;
    }
    return n;
}

// ── Connection ──────────────────────────────────────────────────────────

Connection::~Connection() {
    disconnect();
}

Connection::Connection(Connection&& other) noexcept
    : index_(other.index_) {
    other.index_ = -1;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        disconnect();
        index_ = other.index_;
        other.index_ = -1;
    }
    return *this;
}

void Connection::disconnect() {
    if (index_ >= 0) {
        ConnectionPool::instance().release(index_);
        index_ = -1;
    }
}

} // namespace embed
