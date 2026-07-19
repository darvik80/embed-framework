#include "embed_core/metrics_service.hpp"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

#if CONFIG_EMBED_METRICS_ENABLE_STORAGE
#include "esp_partition.h"
#endif

#include <sys/time.h>

namespace embed {

static const char* TAG = "MetricsService";

MetricsService::~MetricsService() {
    stop();
}

void MetricsService::start() {
    ESP_LOGI(TAG, "Starting MetricsService (interval=%dms)", CONFIG_EMBED_METRICS_INTERVAL_MS);

    bootTimeUs_ = esp_timer_get_time();

    // Initialize CPU load baseline
#if CONFIG_EMBED_METRICS_ENABLE_CPU
    prevTotalRunTime_ = portGET_RUN_TIME_COUNTER_VALUE();
    for (BaseType_t core = 0; core < configNUM_CORES; core++) {
        prevIdleRunTime_[core] = ulTaskGetIdleRunTimeCounterForCore(core);
    }
#endif

    const esp_timer_create_args_t args = {
        .callback = timerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "metrics_timer",
    };
    esp_timer_create(&args, &timer_);
    esp_timer_start_periodic(timer_, CONFIG_EMBED_METRICS_INTERVAL_MS * 1000);
}

void MetricsService::stop() {
    if (timer_) {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
        timer_ = nullptr;
    }
}

bool MetricsService::registerCustomMetric(const char* name, CustomMetricCallback cb, void* ctx) {
    if (!cb) return false;

    // Check for duplicate name
    for (uint8_t i = 0; i < customMetricsCount_; i++) {
        if (customMetrics_[i].name == name) {
            ESP_LOGW(TAG, "Custom metric '%s' already registered", name);
            return false;
        }
    }

    if (customMetricsCount_ >= EMBED_MAX_CUSTOM_METRICS) {
        ESP_LOGW(TAG, "Cannot register '%s': max custom metrics (%d) reached",
                 name, EMBED_MAX_CUSTOM_METRICS);
        return false;
    }

    auto& entry = customMetrics_[customMetricsCount_];
    entry.name = name;
    entry.callback = cb;
    entry.context = ctx;
    customMetricsCount_++;

    ESP_LOGI(TAG, "Registered custom metric '%s' [%d/%d]",
             name, customMetricsCount_, EMBED_MAX_CUSTOM_METRICS);
    return true;
}

void MetricsService::timerCallback(void* arg) {
    auto* self = static_cast<MetricsService*>(arg);
    self->collectAndEmit();
}

void MetricsService::collectAndEmit() {
    MetricsCollected msg{};

    // Timestamp
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    msg.timestamp = static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;

    // CPU load
#if CONFIG_EMBED_METRICS_ENABLE_CPU
    msg.cpuUsagePercent = collectCpuLoad();
#endif

    // Memory
    collectMemoryInfo(msg);

    // Storage
#if CONFIG_EMBED_METRICS_ENABLE_STORAGE
    collectStorageInfo(msg);
#endif

    // Uptime
    int64_t nowUs = esp_timer_get_time();
    msg.uptimeSeconds = static_cast<uint32_t>((nowUs - bootTimeUs_) / 1'000'000);

    // WiFi
    collectWifiInfo(msg);

    // Custom
    collectCustomMetrics(msg);

    onMetricsCollected.emit(msg);
}

uint8_t MetricsService::collectCpuLoad() {
#if CONFIG_EMBED_METRICS_ENABLE_CPU
    configRUN_TIME_COUNTER_TYPE currentTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();

    if (prevTotalRunTime_ == 0) {
        prevTotalRunTime_ = currentTotalRunTime;
        return 0;
    }

    configRUN_TIME_COUNTER_TYPE totalDelta = currentTotalRunTime - prevTotalRunTime_;
    prevTotalRunTime_ = currentTotalRunTime;

    if (totalDelta == 0) return static_cast<uint8_t>(lastCpuLoad_);

    float totalIdleFraction = 0.0f;
    for (BaseType_t core = 0; core < configNUM_CORES; core++) {
        configRUN_TIME_COUNTER_TYPE currentIdleRunTime = ulTaskGetIdleRunTimeCounterForCore(core);
        configRUN_TIME_COUNTER_TYPE idleDelta = currentIdleRunTime - prevIdleRunTime_[core];
        prevIdleRunTime_[core] = currentIdleRunTime;
        totalIdleFraction += static_cast<float>(idleDelta) / static_cast<float>(totalDelta);
    }

    float avgIdlePercent = (totalIdleFraction / configNUM_CORES) * 100.0f;
    float cpuLoad = 100.0f - avgIdlePercent;
    if (cpuLoad < 0.0f) cpuLoad = 0.0f;
    if (cpuLoad > 100.0f) cpuLoad = 100.0f;

    // Exponential moving average for smoothing
    lastCpuLoad_ = (lastCpuLoad_ * 0.7f) + (cpuLoad * 0.3f);
    return static_cast<uint8_t>(lastCpuLoad_);
#else
    return 0;
#endif
}

void MetricsService::collectMemoryInfo(MetricsCollected& msg) {
    // Heap (DRAM + PSRAM combined, 8-bit capable)
    msg.freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    msg.minFreeHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    msg.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    // PSRAM
    msg.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    msg.minFreePsram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}

void MetricsService::collectStorageInfo(MetricsCollected& msg) {
#if CONFIG_EMBED_METRICS_ENABLE_STORAGE
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");

    if (partition == nullptr) {
        return;
    }

    msg.storageTotalBytes = partition->size;
    msg.storageUsedBytes = 0;  // Would need wear-levelling mount to determine
#endif
}

void MetricsService::collectWifiInfo(MetricsCollected& msg) {
    wifi_ap_record_t apInfo;
    if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
        msg.wifiConnected = true;
        msg.wifiRssi = apInfo.rssi;

        // Get IP address
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ipInfo;
            if (esp_netif_get_ip_info(netif, &ipInfo) == ESP_OK) {
                snprintf(msg.wifiIp.data(), msg.wifiIp.capacity() + 1,
                         IPSTR, IP2STR(&ipInfo.ip));
            }
        }
    }
}

void MetricsService::collectCustomMetrics(MetricsCollected& msg) {
    msg.customMetricsCount = 0;
    for (uint8_t i = 0; i < customMetricsCount_; i++) {
        auto& reg = customMetrics_[i];
        if (reg.callback) {
            auto& entry = msg.customMetrics[msg.customMetricsCount];
            entry.name = reg.name;
            float value = 0.0f;
            reg.callback(value, reg.context);
            entry.value = value;
            msg.customMetricsCount++;
        }
    }
}

} // namespace embed
