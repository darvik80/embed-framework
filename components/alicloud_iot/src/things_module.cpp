#include "alicloud_iot/things_module.hpp"
#include "alicloud_iot/message_id_generator.hpp"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "Things";

namespace alicloud::iot {

ThingsModule::ThingsModule(embed::MqttService& mqtt,
                            std::string_view productKey,
                            std::string_view deviceName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
{}

void ThingsModule::addServiceInvokeCallback(const std::string& method, ServiceInvokeCallback cb)
{
    serviceInvokeCbs_[method] = std::move(cb);
    ESP_LOGI(TAG, "Registered service callback for method: %s", method.c_str());
}

void ThingsModule::removeServiceInvokeCallback(const std::string& method)
{
    auto it = serviceInvokeCbs_.find(method);
    if (it != serviceInvokeCbs_.end()) {
        serviceInvokeCbs_.erase(it);
        ESP_LOGI(TAG, "Removed service callback for method: %s", method.c_str());
    }
}

bool ThingsModule::subscribeTopics()
{
    if (!mqtt_.isConnected()) {
        ESP_LOGE(TAG, "MQTT not connected");
        return false;
    }

    if (subscribe(buildTopic("thing/service/property/set")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to property/set");
        return false;
    }
    if (subscribe(buildTopic("thing/service/+/+")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to service/+/+");
        return false;
    }
    if (subscribe(buildTopic("thing/property/desired/get_reply")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to desired/get_reply");
        return false;
    }
    if (subscribe(buildTopic("thing/property/desired/delete_reply")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to desired/delete_reply");
        return false;
    }

    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed to Things topics");
    return true;
}

bool ThingsModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildTopic("thing/service/property/set"));
    unsubscribe(buildTopic("thing/service/+/+"));
    unsubscribe(buildTopic("thing/property/desired/get_reply"));
    unsubscribe(buildTopic("thing/property/desired/delete_reply"));

    subscribed_ = false;
    return true;
}

bool ThingsModule::reportProperties(const std::unordered_map<std::string, PropertyData>& properties)
{
    if (!mqtt_.isConnected() || properties.empty()) return false;

    cJSON* params = cJSON_CreateObject();
    if (!params) return false;

    for (const auto& [key, data] : properties) {
        cJSON* propObj = cJSON_CreateObject();
        if (!propObj) { cJSON_Delete(params); return false; }

        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int>)
                cJSON_AddNumberToObject(propObj, "value", v);
            else if constexpr (std::is_same_v<T, double>)
                cJSON_AddNumberToObject(propObj, "value", v);
            else if constexpr (std::is_same_v<T, bool>)
                cJSON_AddBoolToObject(propObj, "value", v);
            else
                cJSON_AddStringToObject(propObj, "value", v.c_str());
        }, data.value);

        if (data.time > 0)
            cJSON_AddNumberToObject(propObj, "time", static_cast<double>(data.time));

        cJSON_AddItemToObject(params, key.c_str(), propObj);
    }

    std::string msgId   = std::to_string(MessageIdGenerator::generate());
    char*       rawJson = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);

    std::string request = buildRequest("thing.event.property.post", rawJson ? rawJson : "{}", msgId);
    free(rawJson);

    std::string topic = buildTopic("thing/event/property/post");
    int id = publish(topic, request);
    if (id < 0) {
        ESP_LOGE(TAG, "Failed to publish properties");
        return false;
    }
    ESP_LOGI(TAG, "Reported properties, msg_id=%d", id);
    return true;
}

bool ThingsModule::postEvent(const std::string& event_id, const EventData& event_data)
{
    if (!mqtt_.isConnected() || event_id.empty()) return false;

    cJSON* params = cJSON_CreateObject();
    if (!params) return false;

    cJSON* valueObj = cJSON_CreateObject();
    if (!valueObj) { cJSON_Delete(params); return false; }

    for (const auto& [k, v] : event_data.params)
        cJSON_AddStringToObject(valueObj, k.c_str(), v.c_str());

    cJSON_AddItemToObject(params, "value", valueObj);
    if (event_data.time > 0)
        cJSON_AddNumberToObject(params, "time", static_cast<double>(event_data.time));

    std::string msgId   = std::to_string(MessageIdGenerator::generate());
    char*       rawJson = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);

    std::string request = buildRequest("thing.event." + event_id + ".post", rawJson ? rawJson : "{}", msgId);
    free(rawJson);

    std::string topic = buildTopic("thing/event/" + event_id + "/post");
    int id = publish(topic, request);
    if (id < 0) {
        ESP_LOGE(TAG, "Failed to post event %s", event_id.c_str());
        return false;
    }
    ESP_LOGI(TAG, "Posted event %s, msg_id=%d", event_id.c_str(), id);
    return true;
}

bool ThingsModule::getDesiredProperties(const std::vector<std::string>& property_ids)
{
    if (!mqtt_.isConnected() || property_ids.empty()) return false;

    cJSON* params = cJSON_CreateArray();
    if (!params) return false;

    for (const auto& pid : property_ids)
        cJSON_AddItemToArray(params, cJSON_CreateString(pid.c_str()));

    std::string msgId   = std::to_string(MessageIdGenerator::generate());
    char*       rawJson = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);

    std::string request = buildRequest("thing.property.desired.get", rawJson ? rawJson : "[]", msgId);
    free(rawJson);

    std::string topic = buildTopic("thing/property/desired/get");
    int id = publish(topic, request);
    if (id < 0) {
        ESP_LOGE(TAG, "Failed to get desired properties");
        return false;
    }
    ESP_LOGI(TAG, "Requested desired properties, msg_id=%d", id);
    return true;
}

bool ThingsModule::deleteDesiredProperties(const std::unordered_map<std::string, std::optional<int>>& properties)
{
    if (!mqtt_.isConnected() || properties.empty()) return false;

    cJSON* params = cJSON_CreateObject();
    if (!params) return false;

    for (const auto& [key, version] : properties) {
        cJSON* propObj = cJSON_CreateObject();
        if (!propObj) { cJSON_Delete(params); return false; }
        if (version.has_value())
            cJSON_AddNumberToObject(propObj, "version", version.value());
        cJSON_AddItemToObject(params, key.c_str(), propObj);
    }

    std::string msgId   = std::to_string(MessageIdGenerator::generate());
    char*       rawJson = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);

    std::string request = buildRequest("thing.property.desired.delete", rawJson ? rawJson : "{}", msgId);
    free(rawJson);

    std::string topic = buildTopic("thing/property/desired/delete");
    int id = publish(topic, request);
    if (id < 0) {
        ESP_LOGE(TAG, "Failed to delete desired properties");
        return false;
    }
    ESP_LOGI(TAG, "Deleted desired properties, msg_id=%d", id);
    return true;
}

void ThingsModule::sendPropertySetResponse(const std::string& message_id, int code, const std::string& message)
{
    if (message_id.empty()) return;

    cJSON* root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "id",      message_id.c_str());
    cJSON_AddNumberToObject(root, "code",    code);
    cJSON_AddStringToObject(root, "message", message.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return;

    publish(buildTopic("thing/service/property/set_reply"), jsonStr);
    free(jsonStr);
    ESP_LOGI(TAG, "Sent property set response: code=%d", code);
}

void ThingsModule::sendServiceResponse(const std::string& message_id, const ServiceResponse& response)
{
    if (message_id.empty()) return;

    cJSON* root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "id",      message_id.c_str());
    cJSON_AddNumberToObject(root, "code",    response.code);
    cJSON_AddStringToObject(root, "message", response.message.c_str());
    cJSON_AddStringToObject(root, "version", response.version.c_str());

    cJSON* dataObj = cJSON_CreateObject();
    for (const auto& [key, val] : response.data) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
                cJSON_AddStringToObject(dataObj, key.c_str(), v.c_str());
            else if constexpr (std::is_same_v<T, int>)
                cJSON_AddNumberToObject(dataObj, key.c_str(), v);
            else if constexpr (std::is_same_v<T, double>)
                cJSON_AddNumberToObject(dataObj, key.c_str(), v);
            else if constexpr (std::is_same_v<T, bool>)
                cJSON_AddBoolToObject(dataObj, key.c_str(), v);
        }, val);
    }
    cJSON_AddItemToObject(root, "data", dataObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return;

    publish(buildTopic("thing/service/property/set_reply"), jsonStr);
    free(jsonStr);
    ESP_LOGI(TAG, "Sent service response: code=%d", response.code);
}

void ThingsModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));
    ESP_LOGI(TAG, "Received: %.*s", static_cast<int>(topic.size()), topic.data());

    if (topic.find("/thing/property/desired/get_reply") != std::string_view::npos)
        handleDesiredPropertyGet(payload);
    else if (topic.find("/thing/property/desired/delete_reply") != std::string_view::npos)
        handleDesiredPropertyDelete(payload);
    else if (topic.find("/thing/service/property/set_reply") != std::string_view::npos)
        ; // ignore own reply
    else if (topic.find("/thing/service/property/set") != std::string_view::npos)
        handlePropertySet(payload);
    else if (topic.find("/thing/service/") != std::string_view::npos) {
        if (topic.find("/_reply") == std::string_view::npos)
            handleServiceInvoke(topic, payload);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ThingsModule::handlePropertySet(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse property/set JSON");
        return;
    }

    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    if (!idItem || !cJSON_IsString(idItem)) {
        cJSON_Delete(root);
        return;
    }
    std::string msgId = idItem->valuestring;

    std::unordered_map<std::string, std::string> properties;
    cJSON* params = cJSON_GetObjectItem(root, "params");
    if (params && cJSON_IsObject(params)) {
        cJSON* p = nullptr;
        cJSON_ArrayForEach(p, params) {
            if (p->string && cJSON_IsString(p))
                properties[p->string] = p->valuestring;
        }
    }
    cJSON_Delete(root);

    bool ok = true;
    if (propertySetCb_)
        ok = propertySetCb_(properties, msgId);

    sendPropertySetResponse(msgId, ok ? 200 : 500, ok ? "success" : "property set failed");
}

void ThingsModule::handleServiceInvoke(std::string_view topic, std::string_view payload)
{
    size_t pos = topic.find("/thing/service/");
    if (pos == std::string_view::npos) return;

    std::string servicePath(topic.substr(pos + 15));
    // strip /_reply suffix if present
    auto replyPos = servicePath.find("/_reply");
    if (replyPos != std::string::npos)
        servicePath.resize(replyPos);

    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse service invoke JSON");
        return;
    }

    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    std::string msgId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : "";

    std::unordered_map<std::string, VariantValue> params;
    cJSON* paramsJson = cJSON_GetObjectItem(root, "params");
    if (paramsJson && cJSON_IsObject(paramsJson)) {
        cJSON* p = nullptr;
        cJSON_ArrayForEach(p, paramsJson) {
            if (!p->string) continue;
            if (cJSON_IsString(p))
                params[p->string] = std::string(p->valuestring);
            else if (cJSON_IsNumber(p)) {
                if (p->valuedouble == static_cast<double>(p->valueint))
                    params[p->string] = p->valueint;
                else
                    params[p->string] = p->valuedouble;
            } else if (cJSON_IsBool(p))
                params[p->string] = cJSON_IsTrue(p) != 0;
        }
    }
    cJSON_Delete(root);

    ServiceResponse response;
    auto it = serviceInvokeCbs_.find(servicePath);
    if (it != serviceInvokeCbs_.end()) {
        response = it->second(params, msgId);
    } else {
        response.code    = 501;
        response.message = "Service not implemented: " + servicePath;
        ESP_LOGW(TAG, "No callback for service: %s", servicePath.c_str());
    }

    sendServiceResponse(msgId, response);
}

void ThingsModule::handleDesiredPropertyGet(std::string_view payload)
{
    ServiceResponse resp = parseResponse(payload);
    if (resp.code == 200 && desiredCb_) {
        std::unordered_map<std::string, PropertyData> props;
        desiredCb_(props);
    }
}

void ThingsModule::handleDesiredPropertyDelete(std::string_view payload)
{
    ServiceResponse resp = parseResponse(payload);
    if (resp.code == 200)
        ESP_LOGI(TAG, "Desired properties deleted successfully");
    else
        ESP_LOGE(TAG, "Failed to delete desired properties: code=%d", resp.code);
}

ServiceResponse ThingsModule::parseResponse(std::string_view payload)
{
    ServiceResponse response;
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) {
        response.code    = -1;
        response.message = "Invalid JSON";
        return response;
    }

    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    if (codeItem && cJSON_IsNumber(codeItem))
        response.code = codeItem->valueint;

    cJSON* msgItem = cJSON_GetObjectItem(root, "message");
    if (msgItem && cJSON_IsString(msgItem))
        response.message = msgItem->valuestring;

    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    if (idItem && cJSON_IsString(idItem))
        response.id = idItem->valuestring;

    cJSON* verItem = cJSON_GetObjectItem(root, "version");
    if (verItem && cJSON_IsString(verItem))
        response.version = verItem->valuestring;

    cJSON* dataItem = cJSON_GetObjectItem(root, "data");
    if (dataItem && cJSON_IsObject(dataItem)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, dataItem) {
            if (!item->string) continue;
            if (cJSON_IsString(item))
                response.data[item->string] = std::string(item->valuestring);
            else if (cJSON_IsNumber(item)) {
                if (item->valuedouble == static_cast<double>(item->valueint))
                    response.data[item->string] = item->valueint;
                else
                    response.data[item->string] = item->valuedouble;
            } else if (cJSON_IsBool(item))
                response.data[item->string] = cJSON_IsTrue(item) != 0;
        }
    }

    cJSON_Delete(root);
    return response;
}

std::string ThingsModule::buildRequest(const std::string& method,
                                        const std::string& params,
                                        const std::string& message_id)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) return "{}";

    cJSON_AddStringToObject(root, "id",      message_id.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  method.c_str());

    cJSON* paramsObj = cJSON_Parse(params.c_str());
    cJSON_AddItemToObject(root, "params", paramsObj ? paramsObj : cJSON_CreateObject());

    cJSON* sysObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sysObj, "ack", 0);
    cJSON_AddItemToObject(root, "sys", sysObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    std::string result = jsonStr ? jsonStr : "{}";
    free(jsonStr);
    return result;
}

} // namespace alicloud::iot
