#pragma once

#include "embed/config.hpp"
#include "embed/service.hpp"

#include <array>
#include <cstddef>
#include <new>
#include <type_traits>

#if EMBED_THREAD_SAFE
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

namespace embed {

// Lock/unlock macros — must be defined before class that uses them
#if EMBED_THREAD_SAFE
#define EMBED_LOCK() lock()
#define EMBED_UNLOCK() unlock()
#else
#define EMBED_LOCK() ((void)0)
#define EMBED_UNLOCK() ((void)0)
#endif

/// Central registry for all services.
///
/// Services are created once and live for the entire program lifetime.
/// Retrieval is done via dynamic_cast<T*> which requires RTTI.
///
/// Usage:
///   auto* svc = ServiceRegistry::instance().createService<MyService>(arg1, arg2);
///   auto* svc2 = ServiceRegistry::instance().getService<MyService>();
class ServiceRegistry {
public:
    /// Singleton access.
    static ServiceRegistry& instance();

    /// Create and register a service of type T.
    /// Storage is allocated from a static pool (no heap).
    /// Args are forwarded to T's constructor.
    /// Returns pointer to the created service, or nullptr if the pool is full.
    /// If a service of type T already exists, returns the existing one.
    template<typename T, typename... Args>
        requires std::derived_from<T, Service>
    T* createService(Args&&... args) {
        EMBED_LOCK();

        // Check if this type already exists
        for (auto& entry : services_) {
            if (entry.occupied && dynamic_cast<T*>(entry.service) != nullptr) {
                EMBED_UNLOCK();
                return static_cast<T*>(entry.service);
            }
        }

        // Find a free slot
        for (size_t i = 0; i < services_.size(); ++i) {
            if (!services_[i].occupied) {
                static_assert(alignof(T) <= kServiceAlign,
                    "Service alignment exceeds kServiceAlign");
                static_assert(sizeof(T) <= kServiceSize,
                    "Service size exceeds kServiceSize — increase EMBED_SERVICE_SIZE");
                void* buf = static_cast<void*>(&storage_[i]);
                T* svc = new (buf) T(std::forward<Args>(args)...);
                services_[i].service = svc;
                services_[i].occupied = true;
                EMBED_UNLOCK();
                return svc;
            }
        }

        EMBED_UNLOCK();
        return nullptr; // Pool exhausted
    }

    /// Retrieve a service by type using dynamic_cast.
    /// Returns nullptr if no service of type T is registered.
    template<typename T>
        requires std::derived_from<T, Service>
    T* getService() {
        EMBED_LOCK();
        T* found = nullptr;
        for (auto& entry : services_) {
            if (entry.occupied) {
                auto* casted = dynamic_cast<T*>(entry.service);
                if (casted != nullptr) {
                    found = casted;
                    break;
                }
            }
        }
        EMBED_UNLOCK();
        return found;
    }

    /// Check if a service of type T is registered.
    template<typename T>
        requires std::derived_from<T, Service>
    bool hasService() {
        return getService<T>() != nullptr;
    }

    /// Number of currently registered services.
    size_t count() const;

    /// Call start() on all registered services, in creation order.
    /// At this point, all services exist in the registry, so
    /// getService<T>() and signal/slot connections are safe.
    void startAll();

    /// Call stop() on all registered services, in reverse creation order.
    void stopAll();

private:
    ServiceRegistry();
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    struct Entry {
        Service* service = nullptr;
        bool occupied = false;
    };

    // Per-slot storage with explicit alignment.
    // Each slot can hold any object up to EMBED_SERVICE_SIZE bytes.
    static constexpr size_t kServiceAlign = alignof(std::max_align_t);
    static constexpr size_t kServiceSize = EMBED_SERVICE_SIZE;

    struct alignas(kServiceAlign) ServiceStorage {
        unsigned char data[kServiceSize];
    };

    std::array<ServiceStorage, EMBED_MAX_SERVICES> storage_{};
    std::array<Entry, EMBED_MAX_SERVICES> services_{};

#if EMBED_THREAD_SAFE
    mutable SemaphoreHandle_t mutex_ = nullptr;
    void lock() const;
    void unlock() const;
#else
    void lock() const {}
    void unlock() const {}
#endif
};

} // namespace embed
