#include "embed/connection.hpp"
#include "embed/event_loop.hpp"

namespace embed {

// ── ConnectionPool ──────────────────────────────────────────────────────

ConnectionPool& ConnectionPool::instance() {
    static ConnectionPool pool;
    return pool;
}

ConnectionPool::ConnectionPool() {
#if EMBED_THREAD_SAFE
    mutex_ = xSemaphoreCreateMutex();
#endif
}

#if EMBED_THREAD_SAFE
void ConnectionPool::lock() const {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
}

void ConnectionPool::unlock() const {
    if (mutex_) xSemaphoreGive(mutex_);
}
#endif

Connection ConnectionPool::allocate(esp_event_base_t base,
                                    int32_t id,
                                    esp_event_handler_instance_t instance) {
#if EMBED_THREAD_SAFE
    lock();
#endif
    Connection result{};
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (!entries_[i].active) {
            entries_[i] = {base, id, instance, true};
            result = Connection{i};
            break;
        }
    }
#if EMBED_THREAD_SAFE
    unlock();
#endif
    return result;
}

void ConnectionPool::release(int index) {
#if EMBED_THREAD_SAFE
    lock();
#endif
    if (index < 0 || index >= static_cast<int>(entries_.size()) || !entries_[index].active) {
#if EMBED_THREAD_SAFE
        unlock();
#endif
        return;
    }

    auto& entry = entries_[index];
    esp_event_base_t base = entry.base;
    int32_t id = entry.id;
    esp_event_handler_instance_t instance = entry.instance;

    entry.active = false;
    entry.base = nullptr;
    entry.id = 0;
    entry.instance = nullptr;
#if EMBED_THREAD_SAFE
    unlock();
#endif

    // Unregister outside the pool lock to avoid nesting with EventLoop work.
    if (instance) {
        EventLoop::instance().unregisterHandler(base, id, instance);
    }
}

size_t ConnectionPool::count() const {
#if EMBED_THREAD_SAFE
    lock();
#endif
    size_t n = 0;
    for (const auto& entry : entries_) {
        if (entry.active) ++n;
    }
#if EMBED_THREAD_SAFE
    unlock();
#endif
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
