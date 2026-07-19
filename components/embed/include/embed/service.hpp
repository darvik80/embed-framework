#pragma once

namespace embed {

/// Base class for all services in the framework.
///
/// A service is a long-lived singleton that provides functionality
/// to other services via the registry and communicates via signals/slots.
///
/// Lifecycle:
///   1. Constructor — called by ServiceRegistry::createService()
///   2. start()     — called by ServiceRegistry::startAll()
///   3. stop()      — called by ServiceRegistry::stopAll()
///
/// In start(), all other services are guaranteed to exist in the registry,
/// so getService<T>() is safe to call.
///
/// The virtual destructor makes this class polymorphic, which is required
/// for dynamic_cast in getService<T>().
class Service {
public:
    virtual ~Service() = default;

    /// Returns a human-readable name for debugging/logging.
    virtual const char* serviceName() const = 0;

    /// Called by ServiceRegistry::startAll() to start the service.
    /// Override to initialize hardware, start timers, connect to networks, etc.
    /// At this point, all services are created and getService<T>() is safe.
    virtual void start() {}

    /// Called by ServiceRegistry::stopAll() to stop the service.
    /// Override to release hardware, stop timers, disconnect, etc.
    virtual void stop() {}

    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;

protected:
    Service() = default;
};

} // namespace embed
