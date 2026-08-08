#include "crearts_iot/crearts_iot_service.hpp"
#include "crearts_iot/rpc_params.hpp"

#include "embed/registry.hpp"
#include "cJSON.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>

namespace crearts::iot {

static const char* TAG = "CreartsIot";

CreartsIotService::CreartsIotService(const CreartsCredentials& credentials)
    : credentials_(&credentials)
    , topics_(credentials.productId(), credentials.deviceId(), credentials.topicStyle())
{}

void CreartsIotService::start()
{
    mqtt_ = embed::ServiceRegistry::instance().getService<embed::MqttService>();
    if (!mqtt_) {
        ESP_LOGE(TAG, "MqttService not found in registry");
        return;
    }

    mqttConnectedSlot_.connect(mqtt_->onConnected);
    mqttDisconnectedSlot_.connect(mqtt_->onDisconnected);
    mqttMessageSlot_.connect(mqtt_->onMessage);

    ESP_LOGI(TAG, "Started (style=%s product=%.*s device=%.*s)",
             topics_.style() == TopicStyle::Short ? "short" : "full",
             static_cast<int>(topics_.productId().size()), topics_.productId().data(),
             static_cast<int>(topics_.deviceId().size()), topics_.deviceId().data());
}

void CreartsIotService::stop()
{
    mqttConnectedSlot_.disconnect();
    mqttDisconnectedSlot_.disconnect();
    mqttMessageSlot_.disconnect();
    unsubscribeAll();
    mqtt_ = nullptr;
    ESP_LOGI(TAG, "Stopped");
}

uint32_t CreartsIotService::allocRequestId()
{
    const uint32_t id = nextRequestId_++;
    if (nextRequestId_ == 0) nextRequestId_ = 1;
    return id;
}

int CreartsIotService::publishRaw(const std::string& topic,
                                  std::string_view json,
                                  int qos,
                                  bool retain)
{
    if (!mqtt_ || topic.empty() || json.empty()) return -1;
    if (!mqtt_->isConnected()) {
        ESP_LOGW(TAG, "publish: MQTT not connected (%s)", topic.c_str());
        return -1;
    }
    return mqtt_->publish(topic.c_str(), json.data(), static_cast<int>(json.size()), qos, retain);
}

int CreartsIotService::publishTelemetry(std::string_view json, int qos)
{
    return publishRaw(topics_.telemetryPublish(), json, qos);
}

int CreartsIotService::publishTelemetry(const TelemetryBuilder& builder, int qos)
{
    if (builder.empty()) {
        ESP_LOGW(TAG, "publishTelemetry: builder empty");
        return -1;
    }
    const std::string json = builder.build();
    if (json.empty()) return -1;
    return publishTelemetry(std::string_view(json), qos);
}

int CreartsIotService::publishTelemetry(const TelemetryBatch& batch, int qos)
{
    if (batch.empty()) {
        ESP_LOGW(TAG, "publishTelemetry: batch empty");
        return -1;
    }
    const std::string json = batch.build();
    if (json.empty()) return -1;
    return publishTelemetry(std::string_view(json), qos);
}

int CreartsIotService::publishEvents(std::string_view json, int qos)
{
    return publishRaw(topics_.eventsPost(), json, qos);
}

int CreartsIotService::publishAttributes(std::string_view json, int qos)
{
    return publishRaw(topics_.attributesReport(), json, qos);
}

int CreartsIotService::publishAttributes(const AttributeBuilder& builder, int qos)
{
    if (builder.empty()) {
        ESP_LOGW(TAG, "publishAttributes: builder empty");
        return -1;
    }
    const std::string json = builder.build();
    if (json.empty()) return -1;
    return publishAttributes(std::string_view(json), qos);
}

int CreartsIotService::requestAttributes(const AttributeRequestBuilder& request, int qos)
{
    uint32_t unused = 0;
    return requestAttributes(request, unused, qos);
}

int CreartsIotService::requestAttributes(const AttributeRequestBuilder& request,
                                         uint32_t& outRequestId,
                                         int qos)
{
    if (request.empty()) {
        ESP_LOGW(TAG, "requestAttributes: no keys");
        return -1;
    }

    outRequestId = allocRequestId();
    AttributeRequestBuilder withId = request;
    withId.id(outRequestId);
    const std::string json = withId.build();
    if (json.empty()) return -1;
    return publishRaw(topics_.attributesRequest(), json, qos);
}

int CreartsIotService::respondRpc(uint32_t requestId,
                                  int code,
                                  std::string_view message,
                                  std::string_view dataJson,
                                  int qos)
{
    if (requestId == 0) return -1;

    const std::string messageStr(message);
    const std::string dataStr(dataJson);

    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddNumberToObject(root, "id", static_cast<double>(requestId));
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "message", messageStr.c_str());

    if (!dataStr.empty()) {
        cJSON* data = cJSON_ParseWithLength(dataStr.data(), dataStr.size());
        if (data) {
            cJSON_AddItemToObject(root, "data", data);
        } else {
            cJSON_AddNullToObject(root, "data");
        }
    } else {
        cJSON_AddNullToObject(root, "data");
    }

    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return -1;
    const int msgId = publishRaw(topics_.rpcResponse(), printed, qos);
    free(printed);
    return msgId;
}

int CreartsIotService::requestNtp(uint32_t& outRequestId, int64_t deviceSendTimeMs, int qos)
{
    outRequestId = allocRequestId();
    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddNumberToObject(root, "id", static_cast<double>(outRequestId));
    cJSON_AddNumberToObject(root, "deviceSendTime", static_cast<double>(deviceSendTimeMs));
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return -1;
    const int msgId = publishRaw(topics_.ntpRequest(), printed, qos);
    free(printed);
    return msgId;
}

int CreartsIotService::publishOtaVersion(std::string_view version,
                                         std::string_view module,
                                         int qos)
{
    const std::string versionStr(version);
    const std::string moduleStr(module);
    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "version", versionStr.c_str());
    cJSON_AddStringToObject(root, "module", moduleStr.c_str());
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return -1;
    const int msgId = publishRaw(topics_.otaVersion(), printed, qos);
    free(printed);
    return msgId;
}

int CreartsIotService::publishOtaQuery(std::string_view module,
                                       std::string_view version,
                                       int qos)
{
    const std::string moduleStr(module);
    const std::string versionStr(version);
    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "module", moduleStr.c_str());
    if (!versionStr.empty()) {
        cJSON_AddStringToObject(root, "version", versionStr.c_str());
    }
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return -1;
    const int msgId = publishRaw(topics_.otaQuery(), printed, qos);
    free(printed);
    return msgId;
}

int CreartsIotService::publishOtaProgress(std::string_view module,
                                          int step,
                                          std::string_view desc,
                                          int qos)
{
    const std::string moduleStr(module);
    const std::string descStr(desc);
    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "module", moduleStr.c_str());
    cJSON_AddNumberToObject(root, "step", step);
    cJSON_AddStringToObject(root, "desc", descStr.c_str());
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return -1;
    const int msgId = publishRaw(topics_.otaProgress(), printed, qos);
    free(printed);
    return msgId;
}

int CreartsIotService::publishLogs(std::string_view json, int qos)
{
    return publishRaw(topics_.logsReport(), json, qos);
}

void CreartsIotService::onMqttConnected(const embed::MqttConnected& /*msg*/, void* ctx)
{
    auto* self = static_cast<CreartsIotService*>(ctx);
    ESP_LOGI(TAG, "MQTT connected — subscribing downstream");
    self->subscribeAll();
}

void CreartsIotService::onMqttDisconnected(const embed::MqttDisconnected& /*msg*/, void* ctx)
{
    auto* self = static_cast<CreartsIotService*>(ctx);
    ESP_LOGW(TAG, "MQTT disconnected");
    self->subscribed_ = false;
}

void CreartsIotService::onMqttMessage(const embed::MqttMessageReceived& msg, void* ctx)
{
    auto* self = static_cast<CreartsIotService*>(ctx);
    self->handleMessage(msg.topic.c_str(),
                        std::string_view(msg.payload.c_str(), msg.payload.size()));
}

void CreartsIotService::subscribeAll()
{
    if (!mqtt_) return;
    mqtt_->subscribe(topics_.downstreamSubscribe().c_str(), 1);
    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed: %s", topics_.downstreamSubscribe().c_str());
}

void CreartsIotService::unsubscribeAll()
{
    if (!mqtt_ || !subscribed_) return;
    mqtt_->unsubscribe(topics_.downstreamSubscribe().c_str());
    subscribed_ = false;
}

void CreartsIotService::handleMessage(std::string_view topic, std::string_view payload)
{
    // Short-style `v1/#` also delivers device uplinks — ignore them.
    if (Topics::isUplink(topic)) {
        return;
    }

    if (Topics::isRpcRequest(topic)) {
        RpcRequest req{};
        if (!parseRpcRequest(payload, req)) {
            ESP_LOGW(TAG, "RPC request: invalid JSON");
            return;
        }
        ESP_LOGI(TAG, "RPC req id=%lu method=%s",
                 static_cast<unsigned long>(req.requestId), req.method.c_str());
        onRpcRequest.emit(req);
        return;
    }

    if (Topics::isAttributeResponse(topic)) {
        AttributeResponse res{};
        res.requestId = parseJsonId(payload);
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

    if (Topics::isNtpResponse(topic)) {
        NtpResponse res{};
        res.requestId = parseJsonId(payload);
        cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
        if (root) {
            cJSON* a = cJSON_GetObjectItemCaseSensitive(root, "deviceSendTime");
            cJSON* b = cJSON_GetObjectItemCaseSensitive(root, "serverRecvTime");
            cJSON* c = cJSON_GetObjectItemCaseSensitive(root, "serverSendTime");
            if (cJSON_IsNumber(a)) res.deviceSendTime = static_cast<int64_t>(a->valuedouble);
            if (cJSON_IsNumber(b)) res.serverRecvTime = static_cast<int64_t>(b->valuedouble);
            if (cJSON_IsNumber(c)) res.serverSendTime = static_cast<int64_t>(c->valuedouble);
            cJSON_Delete(root);
        }
        onNtpResponse.emit(res);
        return;
    }

    if (Topics::isOtaUpdate(topic)) {
        OtaUpdate upd{};
        upd.payload.assign(payload.data(), payload.size());
        onOtaUpdate.emit(upd);
        return;
    }

    if (Topics::isOtaCancel(topic)) {
        OtaCancel cancel{};
        cancel.payload.assign(payload.data(), payload.size());
        onOtaCancel.emit(cancel);
        return;
    }
}

} // namespace crearts::iot
