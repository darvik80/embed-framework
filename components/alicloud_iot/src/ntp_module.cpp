#include "alicloud_iot/ntp_module.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <sys/time.h>

static const char* TAG = "NTP";

namespace alicloud::iot {

NtpModule::NtpModule(embed::MqttService& mqtt,
                     std::string_view    productKey,
                     std::string_view    deviceName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
{}

bool NtpModule::subscribeTopics()
{
    if (!mqtt_.isConnected()) {
        ESP_LOGE(TAG, "MQTT not connected");
        return false;
    }

    std::string responseTopic = buildNtpTopic("response");
    if (subscribe(responseTopic) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to NTP response");
        return false;
    }
    ESP_LOGI(TAG, "Subscribed to: %s", responseTopic.c_str());

    subscribed_ = true;
    requestTimeSync();
    return true;
}

bool NtpModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildNtpTopic("response"));
    subscribed_ = false;
    return true;
}

bool NtpModule::requestTimeSync()
{
    if (!mqtt_.isConnected()) return false;

    deviceSendTime_ = esp_timer_get_time() / 1000; // uptime ms

    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "deviceSendTime", std::to_string(deviceSendTime_).c_str());

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    std::string topic = buildNtpTopic("request");
    int id = publish(topic, jsonStr, 0);
    free(jsonStr);

    if (id < 0) {
        ESP_LOGE(TAG, "Failed to request NTP sync");
        return false;
    }
    ESP_LOGI(TAG, "Requested NTP sync, msg_id=%d", id);
    return true;
}

void NtpModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));
    if (topic.find("/ext/ntp/") != std::string_view::npos &&
        topic.find("/response") != std::string_view::npos)
        handleNtpResponse(payload);
}

std::string NtpModule::buildNtpTopic(std::string_view suffix) const
{
    std::string t;
    t.reserve(10 + productKey_.size() + 1 + deviceName_.size() + 1 + suffix.size());
    t = "/ext/ntp/";
    t += productKey_;
    t += '/';
    t += deviceName_;
    t += '/';
    t += suffix;
    return t;
}

void NtpModule::handleNtpResponse(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse NTP response JSON");
        return;
    }

    auto readTime = [&](const char* key) -> int64_t {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (!item) return 0;
        if (cJSON_IsString(item)) return std::stoll(item->valuestring);
        if (cJSON_IsNumber(item)) return static_cast<int64_t>(item->valuedouble);
        return 0;
    };

    int64_t deviceSendTime = readTime("deviceSendTime");
    int64_t serverRecvTime = readTime("serverRecvTime");
    int64_t serverSendTime = readTime("serverSendTime");
    cJSON_Delete(root);

    // deviceSendTime_ and deviceRecvTime are uptime-based (esp_timer ms), not Unix epoch.
    // serverRecvTime / serverSendTime are Unix epoch ms from the broker.
    // RTT is measured in the uptime domain; serverSendTime + half-RTT = current Unix time.
    int64_t deviceRecvTime = esp_timer_get_time() / 1000; // uptime ms at moment of response
    int64_t rtt            = deviceRecvTime - deviceSendTime_;
    int64_t serverTime     = serverSendTime + rtt / 2;

    ESP_LOGI(TAG, "deviceSend=%lld srvRecv=%lld srvSend=%lld rtt=%lldms → %lld",
             deviceSendTime, serverRecvTime, serverSendTime, rtt, serverTime);

    setSystemTime(serverTime);

    if (ntpCb_)
        ntpCb_(serverTime, deviceSendTime, serverRecvTime, serverSendTime);
}

bool NtpModule::setSystemTime(int64_t serverTimeMs)
{
    struct timeval tv = {
        .tv_sec  = static_cast<time_t>(serverTimeMs / 1000),
        .tv_usec = static_cast<suseconds_t>((serverTimeMs % 1000) * 1000)
    };

    if (settimeofday(&tv, nullptr) != 0) {
        ESP_LOGE(TAG, "Failed to set system time");
        return false;
    }
    ESP_LOGI(TAG, "System time updated to %lld ms", serverTimeMs);
    return true;
}

} // namespace alicloud::iot
