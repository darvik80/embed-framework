#include "alicloud_iot/remote_config_module.hpp"
#include "alicloud_iot/message_id_generator.hpp"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "RemoteCfg";

namespace alicloud::iot {

RemoteConfigModule::RemoteConfigModule(embed::MqttService& mqtt,
                                        std::string_view    productKey,
                                        std::string_view    deviceName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
{}

bool RemoteConfigModule::subscribeTopics()
{
    if (!mqtt_.isConnected()) { ESP_LOGE(TAG, "MQTT not connected"); return false; }

    if (subscribe(buildTopic("thing/config/get_reply")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to config/get_reply");
        return false;
    }
    if (subscribe(buildTopic("thing/config/push")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to config/push");
        return false;
    }

    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed to RemoteConfig topics");
    return true;
}

bool RemoteConfigModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildTopic("thing/config/get_reply"));
    unsubscribe(buildTopic("thing/config/push"));

    subscribed_ = false;
    return true;
}

bool RemoteConfigModule::getConfig(const std::string& configScope, const std::string& getType)
{
    if (!mqtt_.isConnected()) return false;

    cJSON* params = cJSON_CreateObject();
    if (!params) return false;
    cJSON_AddStringToObject(params, "configScope", configScope.c_str());
    cJSON_AddStringToObject(params, "getType",     getType.c_str());

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  "thing.config.get");
    cJSON_AddItemToObject(root, "params", params);

    cJSON* sysObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sysObj, "ack", 0);
    cJSON_AddItemToObject(root, "sys", sysObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("thing/config/get"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to request config"); return false; }
    ESP_LOGI(TAG, "Requested config, msg_id=%d", id);
    return true;
}

void RemoteConfigModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));

    if (topic.find("/thing/config/get_reply") != std::string_view::npos)
        handleConfigGetReply(payload);
    else if (topic.find("/thing/config/push") != std::string_view::npos &&
             topic.find("push_reply") == std::string_view::npos)
        handleConfigPush(payload);
}

void RemoteConfigModule::handleConfigGetReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) { ESP_LOGE(TAG, "Failed to parse config/get_reply JSON"); return; }

    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;

    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    std::string msgId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : "";
    cJSON_Delete(root);

    if (code != 200) { ESP_LOGE(TAG, "Config get failed, code=%d", code); return; }

    RemoteConfigData config = parseConfigData(payload);
    if (configCb_) configCb_(config, msgId);
    ESP_LOGI(TAG, "Received config: configId=%s", config.configId.c_str());
}

void RemoteConfigModule::handleConfigPush(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) { ESP_LOGE(TAG, "Failed to parse config/push JSON"); return; }

    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    std::string msgId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : "";
    cJSON_Delete(root);

    // Acknowledge push
    cJSON* response = cJSON_CreateObject();
    if (response) {
        cJSON_AddStringToObject(response, "id",   msgId.c_str());
        cJSON_AddNumberToObject(response, "code", 200);
        cJSON_AddItemToObject(response, "data",   cJSON_CreateObject());
        char* responseStr = cJSON_PrintUnformatted(response);
        if (responseStr) {
            publish(buildTopic("thing/config/push_reply"), responseStr);
            free(responseStr);
        }
        cJSON_Delete(response);
    }

    RemoteConfigData config = parseConfigData(payload);
    if (configCb_) configCb_(config, msgId);
    ESP_LOGI(TAG, "Received pushed config: configId=%s", config.configId.c_str());
}

RemoteConfigData RemoteConfigModule::parseConfigData(std::string_view payload)
{
    RemoteConfigData config;
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return config;

    cJSON* dataJson = cJSON_GetObjectItem(root, "data");
    if (!dataJson || !cJSON_IsObject(dataJson)) { cJSON_Delete(root); return config; }

    auto getString = [&](const char* key) -> std::string {
        cJSON* item = cJSON_GetObjectItem(dataJson, key);
        return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : "";
    };

    config.configId   = getString("configId");
    config.sign       = getString("sign");
    config.signMethod = getString("signMethod");
    config.url        = getString("url");
    config.getType    = getString("getType");

    cJSON* sizeItem = cJSON_GetObjectItem(dataJson, "configSize");
    if (sizeItem && cJSON_IsNumber(sizeItem))
        config.configSize = static_cast<int64_t>(sizeItem->valuedouble);

    cJSON_Delete(root);
    return config;
}

} // namespace alicloud::iot
