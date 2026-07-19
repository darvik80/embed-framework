#include "alicloud_iot/alicloud_service.hpp"
#include "embed/registry.hpp"
#include "esp_log.h"

static const char* TAG = "AlicloudService";

namespace alicloud::iot {

void AlicloudService::start()
{
    // Retrieve MqttService from the registry
    mqtt_ = embed::ServiceRegistry::instance().getService<embed::MqttService>();
    if (!mqtt_) {
        ESP_LOGE(TAG, "MqttService not found in registry!");
        return;
    }

    ESP_LOGI(TAG, "Creating Alink modules");

    things_        = std::make_unique<ThingsModule>(*mqtt_, productKey_, deviceName_);
    ota_           = std::make_unique<OtaModule>(*mqtt_, productKey_, deviceName_, "1.0.0");
    ntp_           = std::make_unique<NtpModule>(*mqtt_, productKey_, deviceName_);
    remoteConfig_  = std::make_unique<RemoteConfigModule>(*mqtt_, productKey_, deviceName_);
    deviceTags_    = std::make_unique<DeviceTagsModule>(*mqtt_, productKey_, deviceName_);
    deviceLog_     = std::make_unique<DeviceLogModule>(*mqtt_, productKey_, deviceName_);
    networkStatus_ = std::make_unique<DeviceNetworkStatusModule>(*mqtt_, productKey_, deviceName_);

    // Connect slots to MqttService signals
    mqttConnectedSlot_.connect(mqtt_->onConnected);
    mqttDisconnectedSlot_.connect(mqtt_->onDisconnected);
    mqttMessageSlot_.connect(mqtt_->onMessage);

    ESP_LOGI(TAG, "Started — waiting for MQTT connection");
}

void AlicloudService::stop()
{
    mqttConnectedSlot_.disconnect();
    mqttDisconnectedSlot_.disconnect();
    mqttMessageSlot_.disconnect();

    notifyDisconnected();

    things_.reset();
    ota_.reset();
    ntp_.reset();
    remoteConfig_.reset();
    deviceTags_.reset();
    deviceLog_.reset();
    networkStatus_.reset();

    mqtt_ = nullptr;
    ESP_LOGI(TAG, "Stopped");
}

void AlicloudService::onMqttConnected(const embed::MqttConnected& /*msg*/, void* ctx)
{
    auto* self = static_cast<AlicloudService*>(ctx);
    ESP_LOGI(TAG, "MQTT connected, subscribing all Alink modules");
    self->notifyConnected();
}

void AlicloudService::onMqttDisconnected(const embed::MqttDisconnected& /*msg*/, void* ctx)
{
    auto* self = static_cast<AlicloudService*>(ctx);
    ESP_LOGW(TAG, "MQTT disconnected, unsubscribing all Alink modules");
    self->notifyDisconnected();
}

void AlicloudService::onMqttMessage(const embed::MqttMessageReceived& msg, void* ctx)
{
    auto* self = static_cast<AlicloudService*>(ctx);
    self->dispatchMessage(msg.topic.c_str(),
                          msg.payload.c_str(),
                          static_cast<int>(msg.payload.size()));
}

void AlicloudService::dispatchMessage(std::string_view topic, const char* data, int len)
{
    if (things_)        things_->handleMqttData(topic, data, len);
    if (ota_)           ota_->handleMqttData(topic, data, len);
    if (ntp_)           ntp_->handleMqttData(topic, data, len);
    if (remoteConfig_)  remoteConfig_->handleMqttData(topic, data, len);
    if (deviceTags_)    deviceTags_->handleMqttData(topic, data, len);
    if (deviceLog_)     deviceLog_->handleMqttData(topic, data, len);
    if (networkStatus_) networkStatus_->handleMqttData(topic, data, len);
}

void AlicloudService::notifyConnected()
{
    if (things_)        things_->onConnected();
    if (ota_)           ota_->onConnected();
    if (ntp_)           ntp_->onConnected();
    if (remoteConfig_)  remoteConfig_->onConnected();
    if (deviceTags_)    deviceTags_->onConnected();
    if (deviceLog_)     deviceLog_->onConnected();
    if (networkStatus_) networkStatus_->onConnected();
}

void AlicloudService::notifyDisconnected()
{
    if (things_)        things_->onDisconnected();
    if (ota_)           ota_->onDisconnected();
    if (ntp_)           ntp_->onDisconnected();
    if (remoteConfig_)  remoteConfig_->onDisconnected();
    if (deviceTags_)    deviceTags_->onDisconnected();
    if (deviceLog_)     deviceLog_->onDisconnected();
    if (networkStatus_) networkStatus_->onDisconnected();
}

} // namespace alicloud::iot
