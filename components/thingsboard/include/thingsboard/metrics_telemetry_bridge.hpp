#pragma once

#include "embed/embed.hpp"
#include "embed_core/metrics_service.hpp"
#include "thingsboard/thingsboard_service.hpp"

namespace thingsboard {

/// Forwards embed::MetricsCollected to ThingsBoard telemetry (v2/t).
///
/// Uses client-side timestamp from MetricsCollected::timestamp when non-zero;
/// otherwise publishes a flat KV object (server timestamp).
///
/// Usage:
///   registry.createService<embed::MetricsService>();
///   registry.createService<thingsboard::ThingsBoardService>();
///   registry.createService<thingsboard::MetricsTelemetryBridge>();
class MetricsTelemetryBridge : public embed::Service {
public:
    const char* serviceName() const override { return "MetricsTelemetryBridge"; }

    void start() override;
    void stop() override;

private:
    ThingsBoardService* tb_ = nullptr;

    embed::Slot<embed::MetricsCollected> metricsSlot_{onMetrics, this};
    static void onMetrics(const embed::MetricsCollected& msg, void* ctx);

    static TelemetryBuilder buildFromMetrics(const embed::MetricsCollected& msg);
};

} // namespace thingsboard
