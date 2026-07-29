#include "thingsboard/metrics_telemetry_bridge.hpp"

#include "embed/registry.hpp"
#include "esp_log.h"

namespace thingsboard {

static const char* TAG = "MetricsTbBridge";

void MetricsTelemetryBridge::start()
{
    auto& reg = embed::ServiceRegistry::instance();
    tb_ = reg.getService<ThingsBoardService>();
    auto* metrics = reg.getService<embed::MetricsService>();

    if (!tb_) {
        ESP_LOGE(TAG, "ThingsBoardService not found");
        return;
    }
    if (!metrics) {
        ESP_LOGE(TAG, "MetricsService not found");
        return;
    }

    metricsSlot_.connect(metrics->onMetricsCollected);
    ESP_LOGI(TAG, "Bridging MetricsService → ThingsBoard telemetry");
}

void MetricsTelemetryBridge::stop()
{
    metricsSlot_.disconnect();
    tb_ = nullptr;
}

TelemetryBuilder MetricsTelemetryBridge::buildFromMetrics(const embed::MetricsCollected& msg)
{
    TelemetryBuilder b;
    if (msg.timestamp > 0) {
        b.timestampMs(msg.timestamp);
    }

    b.add("cpuUsagePercent", static_cast<int64_t>(msg.cpuUsagePercent));
    b.add("freeHeap", static_cast<int64_t>(msg.freeHeap));
    b.add("minFreeHeap", static_cast<int64_t>(msg.minFreeHeap));
    b.add("freePsram", static_cast<int64_t>(msg.freePsram));
    b.add("minFreePsram", static_cast<int64_t>(msg.minFreePsram));
    b.add("largestFreeBlock", static_cast<int64_t>(msg.largestFreeBlock));
    b.add("uptimeSeconds", static_cast<int64_t>(msg.uptimeSeconds));
    b.add("storageTotalBytes", static_cast<double>(msg.storageTotalBytes));
    b.add("storageUsedBytes", static_cast<double>(msg.storageUsedBytes));
    b.add("wifiConnected", msg.wifiConnected);
    b.add("wifiRssi", static_cast<int64_t>(msg.wifiRssi));
    if (!msg.wifiIp.empty()) {
        b.add("wifiIp", msg.wifiIp.c_str());
    }

    b.add("version", "1.0.0");
    for (uint8_t i = 0; i < msg.customMetricsCount; ++i) {
        const auto& m = msg.customMetrics[i];
        if (!m.name.empty()) {
            b.add(m.name.c_str(), static_cast<double>(m.value));
        }
    }
    return b;
}

void MetricsTelemetryBridge::onMetrics(const embed::MetricsCollected& msg, void* ctx)
{
    auto* self = static_cast<MetricsTelemetryBridge*>(ctx);
    if (!self->tb_) return;

    TelemetryBuilder b = buildFromMetrics(msg);
    const int id = self->tb_->publishTelemetry(b);
    if (id < 0) {
        ESP_LOGW(TAG, "Failed to publish metrics telemetry");
    }
}

} // namespace thingsboard
