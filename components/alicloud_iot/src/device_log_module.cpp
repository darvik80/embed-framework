#include "alicloud_iot/device_log_module.hpp"
#include "alicloud_iot/message_id_generator.hpp"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "DeviceLog";

namespace alicloud::iot {

DeviceLogModule::DeviceLogModule(embed::MqttService& mqtt,
                                  std::string_view    productKey,
                                  std::string_view    deviceName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
{}

bool DeviceLogModule::subscribeTopics()
{
    if (!mqtt_.isConnected()) { ESP_LOGE(TAG, "MQTT not connected"); return false; }

    if (subscribe(buildTopic("thing/config/log/get_reply")) < 0) return false;
    if (subscribe(buildTopic("thing/config/log/push"))      < 0) return false;
    if (subscribe(buildTopic("thing/log/post_reply"))        < 0) return false;

    subscribed_ = true;
    ESP_LOGI(TAG, "Subscribed to DeviceLog topics");
    return true;
}

bool DeviceLogModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildTopic("thing/config/log/get_reply"));
    unsubscribe(buildTopic("thing/config/log/push"));
    unsubscribe(buildTopic("thing/log/post_reply"));

    subscribed_ = false;
    return true;
}

bool DeviceLogModule::getLogConfig(const std::string& configScope, const std::string& getType)
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
    cJSON_AddStringToObject(root, "method",  "thing.config.log.get");
    cJSON_AddItemToObject(root, "params", params);

    cJSON* sysObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sysObj, "ack", 0);
    cJSON_AddItemToObject(root, "sys", sysObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("thing/config/log/get"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to request log config"); return false; }
    ESP_LOGI(TAG, "Requested log config, msg_id=%d", id);
    return true;
}

bool DeviceLogModule::reportLogs(const std::vector<DeviceLogEntry>& logs)
{
    if (!mqtt_.isConnected() || logs.empty()) return false;
    if (logs.size() > 40) { ESP_LOGE(TAG, "Too many log entries (max 40)"); return false; }

    cJSON* params = cJSON_CreateArray();
    if (!params) return false;

    for (const auto& log : logs) {
        cJSON* obj = cJSON_CreateObject();
        if (!obj) { cJSON_Delete(params); return false; }

        cJSON_AddStringToObject(obj, "utcTime",    log.utcTime.c_str());
        cJSON_AddStringToObject(obj, "logLevel",   logLevelToString(log.logLevel).c_str());
        cJSON_AddStringToObject(obj, "module",     log.module.c_str());
        cJSON_AddStringToObject(obj, "code",       log.code.c_str());
        if (!log.traceContext.empty())
            cJSON_AddStringToObject(obj, "traceContext", log.traceContext.c_str());
        cJSON_AddStringToObject(obj, "logContent", log.logContent.c_str());

        cJSON_AddItemToArray(params, obj);
    }

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON* root = cJSON_CreateObject();
    if (!root) { cJSON_Delete(params); return false; }

    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  "thing.log.post");
    cJSON_AddItemToObject(root, "params", params);

    cJSON* sysObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sysObj, "ack", 0);
    cJSON_AddItemToObject(root, "sys", sysObj);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(buildTopic("thing/log/post"), jsonStr);
    free(jsonStr);

    if (id < 0) { ESP_LOGE(TAG, "Failed to report logs"); return false; }
    ESP_LOGI(TAG, "Reported %zu logs, msg_id=%d", logs.size(), id);
    return true;
}

void DeviceLogModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));

    if (topic.find("/thing/config/log/get_reply") != std::string_view::npos)
        handleLogConfigGetReply(payload);
    else if (topic.find("/thing/config/log/push") != std::string_view::npos)
        handleLogConfigPush(payload);
    else if (topic.find("/thing/log/post_reply") != std::string_view::npos)
        handleLogPostReply(payload);
}

void DeviceLogModule::handleLogConfigGetReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return;

    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;
    cJSON_Delete(root);

    if (code != 200) { ESP_LOGE(TAG, "Log config get failed, code=%d", code); return; }

    LogConfig config = parseLogConfig(payload);
    if (logConfigCb_) logConfigCb_(config);
    ESP_LOGI(TAG, "Received log config: mode=%d", config.mode);
}

void DeviceLogModule::handleLogConfigPush(std::string_view payload)
{
    LogConfig config = parseLogConfig(payload);
    if (logConfigCb_) logConfigCb_(config);
    ESP_LOGI(TAG, "Received pushed log config: mode=%d", config.mode);
}

void DeviceLogModule::handleLogPostReply(std::string_view payload)
{
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return;
    cJSON* codeItem = cJSON_GetObjectItem(root, "code");
    int code = (codeItem && cJSON_IsNumber(codeItem)) ? codeItem->valueint : -1;
    cJSON_Delete(root);
    if (code == 200) ESP_LOGI(TAG, "Log post successful");
    else             ESP_LOGE(TAG, "Log post failed, code=%d", code);
}

LogConfig DeviceLogModule::parseLogConfig(std::string_view payload)
{
    LogConfig config{0};
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return config;

    cJSON* dataJson = cJSON_GetObjectItem(root, "data");
    if (dataJson && cJSON_IsObject(dataJson)) {
        cJSON* contentJson = cJSON_GetObjectItem(dataJson, "content");
        if (contentJson && cJSON_IsObject(contentJson)) {
            cJSON* modeItem = cJSON_GetObjectItem(contentJson, "mode");
            if (modeItem && cJSON_IsNumber(modeItem))
                config.mode = modeItem->valueint;
        }
    }
    cJSON_Delete(root);
    return config;
}

std::string DeviceLogModule::logLevelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
        default:              return "INFO";
    }
}

} // namespace alicloud::iot
