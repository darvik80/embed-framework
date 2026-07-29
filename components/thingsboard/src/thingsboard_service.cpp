#include "thingsboard/thingsboard_service.hpp"

#include "embed/registry.hpp"
#include "cJSON.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>

namespace thingsboard {

static const char* TAG = "ThingsBoardService";

ThingsBoardService::ThingsBoardService(TopicStyle style)
    : topics_(style)
{}

void ThingsBoardService::start()
{
    mqtt_ = embed::ServiceRegistry::instance().getService<embed::MqttService>();
    if (!mqtt_) {
        ESP_LOGE(TAG, "MqttService not found in registry");
        return;
    }

    mqttConnectedSlot_.connect(mqtt_->onConnected);
    mqttDisconnectedSlot_.connect(mqtt_->onDisconnected);
    mqttMessageSlot_.connect(mqtt_->onMessage);

    ESP_LOGI(TAG, "Started (topic style=%s)",
             topics_.style() == TopicStyle::Short ? "short/v2" : "standard/v1");
}

void ThingsBoardService::stop()
{
    mqttConnectedSlot_.disconnect();
    mqttDisconnectedSlot_.disconnect();
    mqttMessageSlot_.disconnect();
    unsubscribeAll();
    mqtt_ = nullptr;
    ESP_LOGI(TAG, "Stopped");
}

int ThingsBoardService::publishTelemetry(std::string_view json, int qos)
{
    if (!mqtt_) return -1;
    const std::string topic = topics_.telemetry();
    return mqtt_->publish(topic.c_str(), json.data(), static_cast<int>(json.size()), qos);
}

int ThingsBoardService::publishAttributes(std::string_view json, int qos)
{
    if (!mqtt_) return -1;
    const std::string topic = topics_.attributes();
    return mqtt_->publish(topic.c_str(), json.data(), static_cast<int>(json.size()), qos);
}

int ThingsBoardService::requestAttributes(std::string_view keysJson, int qos)
{
    if (!mqtt_) return -1;
    const uint32_t id = nextRequestId_++;
    if (nextRequestId_ == 0) nextRequestId_ = 1;
    const std::string topic = topics_.attributesRequest(id);
    return mqtt_->publish(topic.c_str(), keysJson.data(),
                          static_cast<int>(keysJson.size()), qos);
}

int ThingsBoardService::respondRpc(uint32_t requestId, std::string_view jsonPayload, int qos)
{
    if (!mqtt_ || requestId == 0) return -1;
    const std::string topic = topics_.rpcResponse(requestId);
    return mqtt_->publish(topic.c_str(), jsonPayload.data(),
                          static_cast<int>(jsonPayload.size()), qos);
}

void ThingsBoardService::onMqttConnected(const embed::MqttConnected& /*msg*/, void* ctx)
{
    auto* self = static_cast<ThingsBoardService*>(ctx);
    ESP_LOGI(TAG, "MQTT connected — subscribing ThingsBoard topics");
    self->subscribeAll();
}

void ThingsBoardService::onMqttDisconnected(const embed::MqttDisconnected& /*msg*/, void* ctx)
{
    auto* self = static_cast<ThingsBoardService*>(ctx);
    ESP_LOGW(TAG, "MQTT disconnected");
    self->subscribed_ = false;
}

void ThingsBoardService::onMqttMessage(const embed::MqttMessageReceived& msg, void* ctx)
{
    auto* self = static_cast<ThingsBoardService*>(ctx);
    self->handleMessage(msg.topic.c_str(),
                        std::string_view(msg.payload.c_str(), msg.payload.size()));
}

void ThingsBoardService::subscribeAll()
{
    if (!mqtt_) return;

    mqtt_->subscribe(topics_.attributes().c_str(), 1);
    mqtt_->subscribe(topics_.attributesResponseSubscribe().c_str(), 1);
    mqtt_->subscribe(topics_.rpcRequestSubscribe().c_str(), 1);
    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed: attributes + attr responses + RPC requests");
}

void ThingsBoardService::unsubscribeAll()
{
    if (!mqtt_ || !subscribed_) return;
    mqtt_->unsubscribe(topics_.attributes().c_str());
    mqtt_->unsubscribe(topics_.attributesResponseSubscribe().c_str());
    mqtt_->unsubscribe(topics_.rpcRequestSubscribe().c_str());
    subscribed_ = false;
}

void ThingsBoardService::handleMessage(std::string_view topic, std::string_view payload)
{
    if (Topics::isRpcRequest(topic)) {
        RpcRequest req{};
        req.requestId = Topics::parseRpcRequestId(topic);

        cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
        if (root) {
            cJSON* method = cJSON_GetObjectItemCaseSensitive(root, "method");
            cJSON* params = cJSON_GetObjectItemCaseSensitive(root, "params");
            if (cJSON_IsString(method) && method->valuestring) {
                req.method = method->valuestring;
            }
            if (params) {
                char* printed = cJSON_PrintUnformatted(params);
                if (printed) {
                    req.params = printed;
                    free(printed);
                }
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "RPC payload is not JSON, forwarding raw");
            req.params.assign(payload.data(), payload.size());
        }

        ESP_LOGI(TAG, "RPC req id=%lu method=%s",
                 static_cast<unsigned long>(req.requestId), req.method.c_str());
        onRpcRequest.emit(req);
        return;
    }

    if (Topics::isAttributeResponse(topic)) {
        AttributeResponse res{};
        res.requestId = Topics::parseTrailingId(topic);
        res.payload.assign(payload.data(), payload.size());
        onAttributeResponse.emit(res);
        return;
    }

    if (Topics::isAttributeUpdate(topic)) {
        AttributeUpdate upd{};
        upd.payload.assign(payload.data(), payload.size());
        onAttributeUpdate.emit(upd);
        return;
    }
}

} // namespace thingsboard
