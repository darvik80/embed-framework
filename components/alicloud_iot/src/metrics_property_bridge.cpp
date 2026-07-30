#include "alicloud_iot/metrics_property_bridge.hpp"

#include "embed/registry.hpp"
#include "embed_core/metrics_keys.hpp"
#include "esp_log.h"

namespace alicloud::iot {

static const char* TAG = "MetricsAlicloudBridge";

void MetricsPropertyBridge::start()
{
    auto& reg = embed::ServiceRegistry::instance();
    auto* alicloud = reg.getService<AlicloudService>();
    auto* metrics  = reg.getService<embed::MetricsService>();

    if (!alicloud || !alicloud->things()) {
        ESP_LOGE(TAG, "AlicloudService/ThingsModule not found");
        return;
    }
    if (!metrics) {
        ESP_LOGE(TAG, "MetricsService not found");
        return;
    }

    things_ = alicloud->things();
    metricsSlot_.connect(metrics->onMetricsCollected);
    ESP_LOGI(TAG, "Bridging MetricsService → Alicloud properties");
}

void MetricsPropertyBridge::stop()
{
    metricsSlot_.disconnect();
    things_ = nullptr;
}

std::unordered_map<std::string, PropertyData>
MetricsPropertyBridge::buildProperties(const embed::MetricsCollected& msg)
{
    namespace keys = embed::metrics_keys;

    std::unordered_map<std::string, PropertyData> props;
    const int64_t ts = msg.timestamp;

    props[keys::kCpuUsagePercent]   = {static_cast<int>(msg.cpuUsagePercent), ts};
    props[keys::kFreeHeap]          = {static_cast<int>(msg.freeHeap), ts};
    props[keys::kMinFreeHeap]       = {static_cast<int>(msg.minFreeHeap), ts};
    props[keys::kFreePsram]         = {static_cast<int>(msg.freePsram), ts};
    props[keys::kMinFreePsram]      = {static_cast<int>(msg.minFreePsram), ts};
    props[keys::kLargestFreeBlock]  = {static_cast<int>(msg.largestFreeBlock), ts};
    props[keys::kUptimeSeconds]     = {static_cast<int>(msg.uptimeSeconds), ts};
    props[keys::kStorageTotalBytes] = {static_cast<double>(msg.storageTotalBytes), ts};
    props[keys::kStorageUsedBytes]  = {static_cast<double>(msg.storageUsedBytes), ts};
    props[keys::kWifiConnected]     = {msg.wifiConnected, ts};
    props[keys::kWifiRssi]          = {static_cast<int>(msg.wifiRssi), ts};

    if (!msg.wifiIp.empty()) {
        props[keys::kWifiIp] = {std::string(msg.wifiIp.c_str()), ts};
    }

    for (uint8_t i = 0; i < msg.customMetricsCount; ++i) {
        const auto& m = msg.customMetrics[i];
        if (!m.name.empty()) {
            props[std::string(m.name.c_str())] = {static_cast<double>(m.value), ts};
        }
    }

    return props;
}

void MetricsPropertyBridge::onMetrics(const embed::MetricsCollected& msg, void* ctx)
{
    auto* self = static_cast<MetricsPropertyBridge*>(ctx);
    if (!self->things_) return;

    auto props = buildProperties(msg);
    if (!self->things_->reportProperties(props)) {
        ESP_LOGW(TAG, "Failed to report metrics properties");
    }
}

} // namespace alicloud::iot
