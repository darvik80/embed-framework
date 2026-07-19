#include "alicloud_iot/device_network_status_module.hpp"
#include "alicloud_iot/message_id_generator.hpp"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "NetStatus";

static cJSON* buildWifiObject(const alicloud::iot::NetworkStatusData& status)
{
    cJSON* wifiObj = cJSON_CreateObject();
    if (!wifiObj) return nullptr;

    cJSON_AddNumberToObject(wifiObj, "rssi", status.wifi.rssi);
    cJSON_AddNumberToObject(wifiObj, "snr",  status.wifi.snr);
    cJSON_AddNumberToObject(wifiObj, "per",  status.wifi.per);
    if (!status.wifi.errStats.empty())
        cJSON_AddStringToObject(wifiObj, "err_stats", status.wifi.errStats.c_str());

    return wifiObj;
}

namespace alicloud::iot {

DeviceNetworkStatusModule::DeviceNetworkStatusModule(embed::MqttService& mqtt,
                                                      std::string_view    productKey,
                                                      std::string_view    deviceName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
{}

bool DeviceNetworkStatusModule::subscribeTopics()
{
    if (!mqtt_.isConnected()) { ESP_LOGE(TAG, "MQTT not connected"); return false; }

    if (subscribe(buildTopic("_thing/diag/post_reply")) < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to diag/post_reply");
        return false;
    }

    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed to NetworkStatus topics");
    return true;
}

bool DeviceNetworkStatusModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildTopic("_thing/diag/post_reply"));
    subscribed_ = false;
    return true;
}

bool DeviceNetworkStatusModule::reportCurrentStatus(const NetworkStatusData& status)
{
    if (!mqtt_.isConnected()) return false;

    cJSON* params = cJSON_CreateObject();
    if (!params) return false;

    cJSON* pObj = cJSON_CreateObject();
    if (!pObj) { cJSON_Delete(params); return false; }

    cJSON* wifiObj = buildWifiObject(status);
    if (!wifiObj) { cJSON_Delete(pObj); cJSON_Delete(params); return false; }

    cJSON_AddItemToObject(pObj, "wifi",  wifiObj);
    cJSON_AddNumberToObject(pObj, "_time", static_cast<double>(status.timestamp));
    cJSON_AddItemToObject(params, "p", pObj);
    cJSON_AddStringToObject(params, "model", "quantity=single|format=simple|time=now");

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddItemToObject(root, "params", params);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("_thing/diag/post"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to report current status"); return false; }
    ESP_LOGI(TAG, "Reported current network status, msg_id=%d", id);
    return true;
}

bool DeviceNetworkStatusModule::reportHistoryStatus(const std::vector<NetworkStatusData>& statuses)
{
    if (!mqtt_.isConnected() || statuses.empty()) return false;

    cJSON* params = cJSON_CreateObject();
    if (!params) return false;

    cJSON* pArray = cJSON_CreateArray();
    if (!pArray) { cJSON_Delete(params); return false; }

    for (const auto& status : statuses) {
        cJSON* pObj = cJSON_CreateObject();
        if (!pObj) { cJSON_Delete(pArray); cJSON_Delete(params); return false; }

        cJSON* wifiObj = buildWifiObject(status);
        if (!wifiObj) { cJSON_Delete(pObj); cJSON_Delete(pArray); cJSON_Delete(params); return false; }

        cJSON_AddItemToObject(pObj, "wifi",   wifiObj);
        cJSON_AddNumberToObject(pObj, "_time", static_cast<double>(status.timestamp));
        cJSON_AddItemToArray(pArray, pObj);
    }

    cJSON_AddItemToObject(params, "p", pArray);
    cJSON_AddStringToObject(params, "model", "format=simple|quantity=batch|time=history");

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddItemToObject(root, "params", params);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("_thing/diag/post"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to report history status"); return false; }
    ESP_LOGI(TAG, "Reported %zu history status entries, msg_id=%d", statuses.size(), id);
    return true;
}

void DeviceNetworkStatusModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));
    if (topic.find("/_thing/diag/post_reply") != std::string_view::npos)
        handleDiagPostReply(payload);
}

void DeviceNetworkStatusModule::handleDiagPostReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return;
    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;
    cJSON_Delete(root);
    if (code == 200) ESP_LOGI(TAG, "Diag post successful");
    else             ESP_LOGE(TAG, "Diag post failed, code=%d", code);
}

} // namespace alicloud::iot
