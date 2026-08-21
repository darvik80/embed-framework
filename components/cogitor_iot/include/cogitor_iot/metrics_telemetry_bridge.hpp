#pragma once

#include "embed/embed.hpp"
#include "embed_core/metrics_service.hpp"
#include "cogitor_iot/cogitor_iot_service.hpp"

namespace cogitor::iot {

/// Forwards embed::MetricsCollected to Cogitor IoT telemetry.
class MetricsTelemetryBridge : public embed::Service {
public:
    const char* serviceName() const override { return "CogitorMetricsBridge"; }

    void start() override;
    void stop() override;

private:
    IotService* iot_ = nullptr;

    embed::Slot<embed::MetricsCollected> metricsSlot_{onMetrics, this};
    static void onMetrics(const embed::MetricsCollected& msg, void* ctx);

    static TelemetryBuilder buildFromMetrics(const embed::MetricsCollected& msg);
};

} // namespace cogitor::iot
