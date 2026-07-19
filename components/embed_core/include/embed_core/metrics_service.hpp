#pragma once

#include "embed/embed.hpp"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace embed {

// ── Messages ────────────────────────────────────────────────────────────

/// A single named metric value, used in MetricsCollected::customMetrics.
struct CustomMetricEntry {
    embed::string<31> name;
    float value = 0.0f;
};
static_assert(embed::Message<CustomMetricEntry>);

/// Emitted periodically by MetricsService with system metrics.
struct MetricsCollected {
    int64_t timestamp = 0;              // Unix timestamp in ms
    uint8_t cpuUsagePercent = 0;        // CPU load 0-100

    // Memory
    uint32_t freeHeap = 0;
    uint32_t minFreeHeap = 0;
    uint32_t freePsram = 0;
    uint32_t minFreePsram = 0;
    uint32_t largestFreeBlock = 0;

    // Uptime
    uint32_t uptimeSeconds = 0;

    // Storage
    uint64_t storageTotalBytes = 0;
    uint64_t storageUsedBytes = 0;

    // WiFi
    int8_t wifiRssi = 0;
    bool wifiConnected = false;
    embed::string<17> wifiIp;

    // Custom metrics
    uint8_t customMetricsCount = 0;
    embed::array<CustomMetricEntry, EMBED_MAX_CUSTOM_METRICS> customMetrics;
};
static_assert(embed::Message<MetricsCollected>);

// ── MetricsService ──────────────────────────────────────────────────────

/// Service for collecting and emitting system metrics.
///
/// Periodically collects CPU load, memory usage, storage, uptime,
/// WiFi status, and custom metrics. Emits MetricsCollected via signal.
///
/// Custom metrics can be registered with registerCustomMetric().
///
/// Usage:
///   auto* metrics = registry.createService<MetricsService>();
///   metrics->registerCustomMetric("loop_cnt", myCallback, &ctx);
///   registry.startAll();  // starts periodic collection
class MetricsService : public Service {
public:
    const char* serviceName() const override { return "MetricsService"; }

    MetricsService() = default;
    ~MetricsService() override;

    /// Called by ServiceRegistry::startAll().
    /// Creates a periodic timer for metrics collection.
    void start() override;

    /// Called by ServiceRegistry::stopAll().
    /// Stops and deletes the collection timer.
    void stop() override;

    /// Signal emitted after each metrics collection cycle.
    Signal<MetricsCollected> onMetricsCollected;

    /// Callback type for custom metric registration.
    /// The callback should write the metric value to `out`.
    /// `ctx` is the user-provided context pointer.
    using CustomMetricCallback = void(*)(float& out, void* ctx);

    /// Register a custom metric by name.
    /// Returns false if the name already exists or the registry is full.
    bool registerCustomMetric(const char* name, CustomMetricCallback cb, void* ctx = nullptr);

private:
    // Timer
    esp_timer_handle_t timer_ = nullptr;
    static void timerCallback(void* arg);
    void collectAndEmit();

    // Collection helpers
    uint8_t collectCpuLoad();
    void collectMemoryInfo(MetricsCollected& msg);
    void collectStorageInfo(MetricsCollected& msg);
    void collectWifiInfo(MetricsCollected& msg);
    void collectCustomMetrics(MetricsCollected& msg);

    // CPU load tracking (FreeRTOS idle counters)
    int64_t bootTimeUs_ = 0;
    configRUN_TIME_COUNTER_TYPE prevTotalRunTime_ = 0;
    configRUN_TIME_COUNTER_TYPE prevIdleRunTime_[configNUM_CORES] = {};
    float lastCpuLoad_ = 0.0f;

    // Custom metrics registry (fixed-size, no heap)
    struct CustomMetricRegistration {
        embed::string<31> name;
        CustomMetricCallback callback = nullptr;
        void* context = nullptr;
    };
    embed::array<CustomMetricRegistration, EMBED_MAX_CUSTOM_METRICS> customMetrics_{};
    uint8_t customMetricsCount_ = 0;
};

} // namespace embed
