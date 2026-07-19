#include "embed/registry.hpp"

namespace embed {

ServiceRegistry::ServiceRegistry() {
#if EMBED_THREAD_SAFE
    mutex_ = xSemaphoreCreateRecursiveMutex();
#endif
}

ServiceRegistry& ServiceRegistry::instance() {
    static ServiceRegistry registry;
    return registry;
}

size_t ServiceRegistry::count() const {
    size_t n = 0;
    for (const auto& entry : services_) {
        if (entry.occupied) ++n;
    }
    return n;
}

void ServiceRegistry::startAll() {
    EMBED_LOCK();
    for (auto& entry : services_) {
        if (entry.occupied && entry.service) {
            entry.service->start();
        }
    }
    EMBED_UNLOCK();
}

void ServiceRegistry::stopAll() {
    EMBED_LOCK();
    // Stop in reverse creation order
    for (int i = static_cast<int>(services_.size()) - 1; i >= 0; --i) {
        if (services_[i].occupied && services_[i].service) {
            services_[i].service->stop();
        }
    }
    EMBED_UNLOCK();
}

#if EMBED_THREAD_SAFE
void ServiceRegistry::lock() const {
    if (mutex_) {
        xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
    }
}

void ServiceRegistry::unlock() const {
    if (mutex_) {
        xSemaphoreGiveRecursive(mutex_);
    }
}
#endif

} // namespace embed
