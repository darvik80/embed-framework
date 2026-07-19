#include "alicloud_iot/device_tags_module.hpp"
#include "alicloud_iot/message_id_generator.hpp"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "Tags";

namespace alicloud::iot {

DeviceTagsModule::DeviceTagsModule(embed::MqttService& mqtt,
                                    std::string_view    productKey,
                                    std::string_view    deviceName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
{}

bool DeviceTagsModule::subscribeTopics()
{
    if (!mqtt_.isConnected()) { ESP_LOGE(TAG, "MQTT not connected"); return false; }

    if (subscribe(buildTopic("thing/deviceinfo/update_reply")) < 0) return false;
    if (subscribe(buildTopic("thing/deviceinfo/get_reply"))    < 0) return false;
    if (subscribe(buildTopic("thing/deviceinfo/delete_reply")) < 0) return false;

    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed to DeviceTags topics");
    return true;
}

bool DeviceTagsModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildTopic("thing/deviceinfo/update_reply"));
    unsubscribe(buildTopic("thing/deviceinfo/get_reply"));
    unsubscribe(buildTopic("thing/deviceinfo/delete_reply"));

    subscribed_ = false;
    return true;
}

bool DeviceTagsModule::submitTags(const std::vector<DeviceTag>& tags)
{
    if (!mqtt_.isConnected() || tags.empty()) return false;
    if (tags.size() > 200) { ESP_LOGE(TAG, "Too many tags (max 200)"); return false; }

    cJSON* params = cJSON_CreateArray();
    if (!params) return false;

    for (const auto& tag : tags) {
        cJSON* obj = cJSON_CreateObject();
        if (!obj) { cJSON_Delete(params); return false; }
        cJSON_AddStringToObject(obj, "attrKey",   tag.attrKey.c_str());
        cJSON_AddStringToObject(obj, "attrValue", tag.attrValue.c_str());
        cJSON_AddItemToArray(params, obj);
    }

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  "thing.deviceinfo.update");
    cJSON_AddItemToObject(root, "params", params);

    cJSON* sysObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sysObj, "ack", 0);
    cJSON_AddItemToObject(root, "sys", sysObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("thing/deviceinfo/update"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to submit tags"); return false; }
    ESP_LOGI(TAG, "Submitted %zu tags, msg_id=%d", tags.size(), id);
    return true;
}

bool DeviceTagsModule::queryTags(const std::vector<std::string>& attrKeys)
{
    if (!mqtt_.isConnected() || attrKeys.empty()) return false;
    if (attrKeys.size() > 10) { ESP_LOGE(TAG, "Too many keys (max 10)"); return false; }

    cJSON* params    = cJSON_CreateObject();
    cJSON* keysArray = cJSON_CreateArray();
    if (!params || !keysArray) { cJSON_Delete(params); cJSON_Delete(keysArray); return false; }

    for (const auto& key : attrKeys)
        cJSON_AddItemToArray(keysArray, cJSON_CreateString(key.c_str()));
    cJSON_AddItemToObject(params, "attrKeys", keysArray);

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  "thing.deviceinfo.get");
    cJSON_AddItemToObject(root, "params", params);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("thing/deviceinfo/get"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to query tags"); return false; }
    ESP_LOGI(TAG, "Queried %zu tag keys, msg_id=%d", attrKeys.size(), id);
    return true;
}

bool DeviceTagsModule::deleteTags(const std::vector<std::string>& attrKeys)
{
    if (!mqtt_.isConnected() || attrKeys.empty()) return false;

    cJSON* params = cJSON_CreateArray();
    if (!params) return false;

    for (const auto& key : attrKeys) {
        cJSON* obj = cJSON_CreateObject();
        if (!obj) { cJSON_Delete(params); return false; }
        cJSON_AddStringToObject(obj, "attrKey", key.c_str());
        cJSON_AddItemToArray(params, obj);
    }

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  "thing.deviceinfo.delete");
    cJSON_AddItemToObject(root, "params", params);

    cJSON* sysObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sysObj, "ack", 0);
    cJSON_AddItemToObject(root, "sys", sysObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("thing/deviceinfo/delete"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to delete tags"); return false; }
    ESP_LOGI(TAG, "Deleted %zu tags, msg_id=%d", attrKeys.size(), id);
    return true;
}

void DeviceTagsModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));

    if (topic.find("/thing/deviceinfo/update_reply") != std::string_view::npos)
        handleTagUpdateReply(payload);
    else if (topic.find("/thing/deviceinfo/get_reply") != std::string_view::npos)
        handleTagQueryReply(payload);
    else if (topic.find("/thing/deviceinfo/delete_reply") != std::string_view::npos)
        handleTagDeleteReply(payload);
}

void DeviceTagsModule::handleTagUpdateReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return;
    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;
    cJSON_Delete(root);
    if (code == 200) ESP_LOGI(TAG, "Tag update successful");
    else             ESP_LOGE(TAG, "Tag update failed, code=%d", code);
}

void DeviceTagsModule::handleTagQueryReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return;

    cJSON* idItem   = cJSON_GetObjectItem(root, "id");
    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    std::string msgId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : "";
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;

    if (code != 200) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Tag query failed, code=%d", code);
        return;
    }

    std::unordered_map<std::string, std::string> tags;
    cJSON* dataJson = cJSON_GetObjectItem(root, "data");
    if (dataJson && cJSON_IsArray(dataJson)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, dataJson) {
            if (!cJSON_IsObject(item)) continue;
            cJSON* keyItem = cJSON_GetObjectItem(item, "attrKey");
            cJSON* valItem = cJSON_GetObjectItem(item, "attrValue");
            if (keyItem && cJSON_IsString(keyItem) && valItem && cJSON_IsString(valItem))
                tags[keyItem->valuestring] = valItem->valuestring;
        }
    }
    cJSON_Delete(root);

    if (tagQueryCb_) tagQueryCb_(tags, msgId);
    ESP_LOGI(TAG, "Received %zu tags", tags.size());
}

void DeviceTagsModule::handleTagDeleteReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return;
    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;
    cJSON_Delete(root);
    if (code == 200) ESP_LOGI(TAG, "Tag delete successful");
    else             ESP_LOGE(TAG, "Tag delete failed, code=%d", code);
}

} // namespace alicloud::iot
