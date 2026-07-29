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
    if (!mqtt_ || json.empty()) return -1;
    if (!mqtt_->isConnected()) {
        ESP_LOGW(TAG, "publishTelemetry: MQTT not connected");
        return -1;
    }
    const std::string topic = topics_.telemetry();
    int msgId = mqtt_->publish(topic.c_str(), json.data(), static_cast<int>(json.size()), qos);
    if (msgId >= 0) {
        ESP_LOGD(TAG, "telemetry published (%d bytes) msgId=%d",
                 static_cast<int>(json.size()), msgId);
    }
    return msgId;
}

int ThingsBoardService::publishTelemetry(const TelemetryBuilder& builder, int qos)
{
    if (builder.empty()) {
        ESP_LOGW(TAG, "publishTelemetry: builder empty");
        return -1;
    }
    const std::string json = builder.build();
    if (json.empty()) {
        ESP_LOGE(TAG, "publishTelemetry: build failed");
        return -1;
    }
    return publishTelemetry(std::string_view(json), qos);
}

int ThingsBoardService::publishTelemetry(const TelemetryBatch& batch, int qos)
{
    if (batch.empty()) {
        ESP_LOGW(TAG, "publishTelemetry: batch empty");
        return -1;
    }
    const std::string json = batch.build();
    if (json.empty()) {
        ESP_LOGE(TAG, "publishTelemetry: batch build failed");
        return -1;
    }
    return publishTelemetry(std::string_view(json), qos);
}

int ThingsBoardService::publishAttributes(std::string_view json, int qos)
{
    if (!mqtt_ || json.empty()) return -1;
    if (!mqtt_->isConnected()) {
        ESP_LOGW(TAG, "publishAttributes: MQTT not connected");
        return -1;
    }
    const std::string topic = topics_.attributes();
    int msgId = mqtt_->publish(topic.c_str(), json.data(), static_cast<int>(json.size()), qos);
    if (msgId >= 0) {
        ESP_LOGD(TAG, "attributes published (%d bytes) msgId=%d",
                 static_cast<int>(json.size()), msgId);
    }
    return msgId;
}

int ThingsBoardService::publishAttributes(const AttributeBuilder& builder, int qos)
{
    if (builder.empty()) {
        ESP_LOGW(TAG, "publishAttributes: builder empty");
        return -1;
    }
    const std::string json = builder.build();
    if (json.empty()) {
        ESP_LOGE(TAG, "publishAttributes: build failed");
        return -1;
    }
    return publishAttributes(std::string_view(json), qos);
}

uint32_t ThingsBoardService::allocRequestId()
{
    const uint32_t id = nextRequestId_++;
    if (nextRequestId_ == 0) nextRequestId_ = 1;
    return id;
}

int ThingsBoardService::requestAttributes(std::string_view keysJson, int qos)
{
    if (!mqtt_ || keysJson.empty()) return -1;
    if (!mqtt_->isConnected()) {
        ESP_LOGW(TAG, "requestAttributes: MQTT not connected");
        return -1;
    }
    const uint32_t id = allocRequestId();
    const std::string topic = topics_.attributesRequest(id);
    int msgId = mqtt_->publish(topic.c_str(), keysJson.data(),
                               static_cast<int>(keysJson.size()), qos);
    if (msgId >= 0) {
        ESP_LOGI(TAG, "attribute request id=%lu msgId=%d",
                 static_cast<unsigned long>(id), msgId);
    }
    return msgId;
}

int ThingsBoardService::requestAttributes(const AttributeRequestBuilder& request, int qos)
{
    uint32_t unused = 0;
    return requestAttributes(request, unused, qos);
}

int ThingsBoardService::requestAttributes(const AttributeRequestBuilder& request,
                                          uint32_t& outRequestId,
                                          int qos)
{
    if (request.empty()) {
        ESP_LOGW(TAG, "requestAttributes: no keys");
        return -1;
    }
    if (!mqtt_) return -1;
    if (!mqtt_->isConnected()) {
        ESP_LOGW(TAG, "requestAttributes: MQTT not connected");
        return -1;
    }

    const std::string json = request.build();
    if (json.empty()) {
        ESP_LOGE(TAG, "requestAttributes: build failed");
        return -1;
    }

    outRequestId = allocRequestId();
    const std::string topic = topics_.attributesRequest(outRequestId);
    int msgId = mqtt_->publish(topic.c_str(), json.data(),
                               static_cast<int>(json.size()), qos);
    if (msgId >= 0) {
        ESP_LOGI(TAG, "attribute request id=%lu payload=%s",
                 static_cast<unsigned long>(outRequestId), json.c_str());
    }
    return msgId;
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
        ESP_LOGI(TAG, "attribute response id=%lu len=%u",
                 static_cast<unsigned long>(res.requestId),
                 static_cast<unsigned>(res.payload.size()));
        onAttributeResponse.emit(res);
        return;
    }

    if (Topics::isAttributeUpdate(topic)) {
        AttributeUpdate upd{};
        upd.payload.assign(payload.data(), payload.size());
        ESP_LOGI(TAG, "shared attribute update len=%u",
                 static_cast<unsigned>(upd.payload.size()));
        onAttributeUpdate.emit(upd);
        return;
    }
}

} // namespace thingsboard
