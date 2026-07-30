#pragma once

#include "embed/embed.hpp"
#include "embed_core/metrics_service.hpp"
#include "alicloud_iot/alicloud_service.hpp"

namespace alicloud::iot {

/// Forwards embed::MetricsCollected to Alibaba Cloud IoT as device properties
/// via ThingsModule::reportProperties().
///
/// Usage:
///   registry.createService<embed::MetricsService>();
///   registry.createService<alicloud::iot::AlicloudService>();
///   registry.createService<alicloud::iot::MetricsPropertyBridge>();
class MetricsPropertyBridge : public embed::Service {
public:
    const char* serviceName() const override { return "MetricsPropertyBridge"; }

    void start() override;
    void stop() override;

private:
    ThingsModule* things_ = nullptr;

    embed::Slot<embed::MetricsCollected> metricsSlot_{onMetrics, this};
    static void onMetrics(const embed::MetricsCollected& msg, void* ctx);

    static std::unordered_map<std::string, PropertyData> buildProperties(const embed::MetricsCollected& msg);
};

} // namespace alicloud::iot
