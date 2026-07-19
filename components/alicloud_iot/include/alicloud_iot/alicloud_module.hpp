#pragma once

#include "embed_core/mqtt_service.hpp"
#include "esp_log.h"
#include <string>
#include <string_view>

namespace alicloud::iot {

/**
 * @brief Base class for all Alibaba Cloud IoT Alink protocol modules.
 *
 * Provides common functionality:
 * - topic building (/sys/{product}/{device}/suffix)
 * - MQTT publish/subscribe/unsubscribe helpers
 * - lifecycle callbacks (onConnected / onDisconnected)
 *
 * Derived classes must implement:
 * - subscribeTopics()
 * - unsubscribeTopics()
 * - handleMqttData(topic, data, data_len)
 */
class AlicloudBaseModule {
public:
    explicit AlicloudBaseModule(embed::MqttService& mqtt,
                                 const char* moduleName,
                                 std::string_view productKey,
                                 std::string_view deviceName)
        : mqtt_(mqtt)
        , moduleName_(moduleName)
        , productKey_(productKey)
        , deviceName_(deviceName)
    {}

    virtual ~AlicloudBaseModule() = default;

    /// Called by AlicloudService when MQTT connects. Subscribes to topics.
    virtual void onConnected() {
        ESP_LOGI(moduleName_, "MQTT connected, subscribing to topics");
        if (!subscribeTopics()) {
            ESP_LOGE(moduleName_, "Failed to subscribe to topics");
        }
    }

    /// Called by AlicloudService when MQTT disconnects. Unsubscribes from topics.
    virtual void onDisconnected() {
        ESP_LOGW(moduleName_, "MQTT disconnected, unsubscribing from topics");
        unsubscribeTopics();
    }

    /// Called by AlicloudService for every inbound MQTT message.
    virtual void handleMqttData(std::string_view topic,
                                 const char* data,
                                 int data_len) = 0;

    // Non-copyable
    AlicloudBaseModule(const AlicloudBaseModule&)            = delete;
    AlicloudBaseModule& operator=(const AlicloudBaseModule&) = delete;

protected:
    embed::MqttService& mqtt_;
    const char*         moduleName_;
    std::string_view    productKey_;
    std::string_view    deviceName_;
    bool                subscribed_ = false;

    /// Subscribe to required MQTT topics. Returns true on success.
    virtual bool subscribeTopics()   = 0;

    /// Unsubscribe from all MQTT topics.
    virtual bool unsubscribeTopics() = 0;

    /// Build a /sys/{product}/{device}/{suffix} topic.
    std::string buildTopic(std::string_view suffix) const {
        std::string t;
        t.reserve(4 + productKey_.size() + 1 + deviceName_.size() + 1 + suffix.size() + 5);
        t = "/sys/";
        t += productKey_;
        t += '/';
        t += deviceName_;
        t += '/';
        t += suffix;
        return t;
    }

    /// Publish payload to topic. Returns MQTT message ID, or -1 on failure.
    int publish(std::string_view topic, std::string_view payload, int qos = 1) {
        if (!mqtt_.isConnected()) {
            ESP_LOGE(moduleName_, "MQTT not connected, cannot publish");
            return -1;
        }
        return mqtt_.publish(std::string(topic).c_str(),
                             payload.data(),
                             static_cast<int>(payload.size()),
                             qos);
    }

    /// Subscribe to a topic. Returns message ID, or -1 on failure.
    int subscribe(std::string_view topic, int qos = 1) {
        return mqtt_.subscribe(std::string(topic).c_str(), qos);
    }

    /// Unsubscribe from a topic.
    int unsubscribe(std::string_view topic) {
        return mqtt_.unsubscribe(std::string(topic).c_str());
    }
};

} // namespace alicloud::iot
