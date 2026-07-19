#include "alicloud_iot/ota_module.hpp"
#include "alicloud_iot/message_id_generator.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/md5.h"
#include "cJSON.h"
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>

static const char* TAG = "OTA";
static constexpr int OTA_HTTP_BUFFER_SIZE = 4096;

namespace alicloud::iot {

OtaModule::OtaModule(embed::MqttService& mqtt,
                     std::string_view    productKey,
                     std::string_view    deviceName,
                     std::string         currentVersion,
                     std::string         moduleName)
    : AlicloudBaseModule(mqtt, TAG, productKey, deviceName)
    , currentVersion_(std::move(currentVersion))
    , moduleName_(std::move(moduleName))
{
    ESP_LOGI(TAG, "OtaModule initialized, version: %s", currentVersion_.c_str());
}

OtaModule::~OtaModule()
{
    unsubscribeTopics();
}

void OtaModule::setFirmwareCallback(OtaFirmwareCallback cb)
{
    firmwareCb_ = std::move(cb);
}

bool OtaModule::reportVersion()
{
    std::string topic = buildOtaTopic("inform");

    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON_AddStringToObject(root, "id", msgId.c_str());

    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "version", currentVersion_.c_str());
    if (!moduleName_.empty())
        cJSON_AddStringToObject(params, "module", moduleName_.c_str());
    cJSON_AddItemToObject(root, "params", params);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(topic, jsonStr);
    free(jsonStr);

    if (id < 0) {
        ESP_LOGE(TAG, "Failed to report version");
        return false;
    }
    ESP_LOGI(TAG, "Reported version: %s", currentVersion_.c_str());
    return true;
}

void OtaModule::checkRollbackState(std::function<bool()> diagnosticFn)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t   state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;

    ESP_LOGI(TAG, "OTA image pending verification");
    if (diagnosticFn && diagnosticFn()) {
        ESP_LOGI(TAG, "Diagnostics passed, confirming image");
        esp_ota_mark_app_valid_cancel_rollback();
        reportVersion();
    } else {
        ESP_LOGE(TAG, "Diagnostics failed, rolling back");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

bool OtaModule::queryFirmware()
{
    std::string topic = buildTopic("thing/ota/firmware/get");

    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON_AddStringToObject(root, "id",      msgId.c_str());
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddStringToObject(root, "method",  "thing.ota.firmware.get");

    cJSON* params = cJSON_CreateObject();
    if (!moduleName_.empty())
        cJSON_AddStringToObject(params, "module", moduleName_.c_str());
    cJSON_AddItemToObject(root, "params", params);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return false;

    int id = publish(topic, jsonStr);
    free(jsonStr);

    if (id < 0) {
        ESP_LOGE(TAG, "Failed to query firmware");
        return false;
    }
    ESP_LOGI(TAG, "Sent firmware query, msg_id=%d", id);
    return true;
}

bool OtaModule::subscribeTopics()
{
    if (subscribed_) return true;

    std::string upgradeTopic  = buildOtaTopic("upgrade");
    std::string getReplyTopic = buildTopic("thing/ota/firmware/get_reply");

    if (subscribe(upgradeTopic)  < 0) { ESP_LOGE(TAG, "Failed to subscribe: %s", upgradeTopic.c_str());  return false; }
    if (subscribe(getReplyTopic) < 0) { ESP_LOGE(TAG, "Failed to subscribe: %s", getReplyTopic.c_str()); return false; }

    subscribed_ = true;
    reportVersion();
    return true;
}

bool OtaModule::unsubscribeTopics()
{
    if (!subscribed_) return true;

    unsubscribe(buildOtaTopic("upgrade"));
    unsubscribe(buildTopic("thing/ota/firmware/get_reply"));

    subscribed_ = false;
    return true;
}

void OtaModule::handleMqttData(std::string_view topic, const char* data, int data_len)
{
    std::string_view payload(data, static_cast<size_t>(data_len));

    if (topic.find("/ota/device/upgrade/") != std::string_view::npos)
        handleFirmwareUpgrade(payload);
    else if (topic.find("/thing/ota/firmware/get_reply") != std::string_view::npos)
        handleFirmwareGetReply(payload);
}

std::string OtaModule::buildOtaTopic(const std::string& suffix) const
{
    std::ostringstream oss;
    oss << "/ota/device/" << suffix << "/" << productKey_ << "/" << deviceName_;
    return oss.str();
}

void OtaModule::handleFirmwareUpgrade(std::string_view payload)
{
    ESP_LOGI(TAG, "Received firmware upgrade push");

    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) { ESP_LOGE(TAG, "Failed to parse upgrade JSON"); return; }

    cJSON* codeItem = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsString(codeItem) || strcmp(codeItem->valuestring, "1000") != 0) {
        ESP_LOGE(TAG, "Unexpected code in firmware upgrade message");
        cJSON_Delete(root);
        return;
    }

    cJSON* dataItem = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(dataItem)) {
        cJSON_Delete(root);
        return;
    }

    OtaFirmwareInfo firmware = parseFirmwareInfo(dataItem);
    cJSON_Delete(root);

    if (firmware.url.empty()) { ESP_LOGE(TAG, "Firmware URL missing"); return; }
    if (firmwareCb_ && !firmwareCb_(firmware)) { ESP_LOGW(TAG, "Firmware update rejected"); return; }

    performOtaUpdate(firmware);
}

void OtaModule::handleFirmwareGetReply(std::string_view payload)
{
    ESP_LOGI(TAG, "Received firmware query reply");

    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) { ESP_LOGE(TAG, "Failed to parse get_reply JSON"); return; }

    cJSON* codeItem = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(codeItem) || codeItem->valueint != 200) {
        cJSON_Delete(root);
        return;
    }

    cJSON* dataItem = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(dataItem) || cJSON_GetArraySize(dataItem) == 0) {
        ESP_LOGI(TAG, "No firmware update available");
        cJSON_Delete(root);
        return;
    }

    OtaFirmwareInfo firmware = parseFirmwareInfo(dataItem);
    cJSON_Delete(root);

    if (firmware.url.empty()) return;
    if (firmwareCb_ && !firmwareCb_(firmware)) { ESP_LOGW(TAG, "Firmware update rejected"); return; }

    performOtaUpdate(firmware);
}

OtaFirmwareInfo OtaModule::parseFirmwareInfo(const void* dataJsonObject) const
{
    OtaFirmwareInfo info;
    const cJSON* data = static_cast<const cJSON*>(dataJsonObject);

    auto getString = [&](const char* key) -> std::string {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(data, key);
        return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : "";
    };

    info.version    = getString("version");
    info.url        = getString("url");
    info.md5        = getString("md5");
    info.sign       = getString("sign");
    info.signMethod = getString("signMethod");
    info.module     = getString("module");

    cJSON* sizeItem   = cJSON_GetObjectItemCaseSensitive(data, "size");
    cJSON* isDiffItem = cJSON_GetObjectItemCaseSensitive(data, "isDiff");

    if (cJSON_IsNumber(sizeItem))
        info.size = static_cast<int32_t>(sizeItem->valuedouble);
    if (cJSON_IsNumber(isDiffItem))
        info.isDiff = isDiffItem->valueint != 0;

    return info;
}

void OtaModule::performOtaUpdate(const OtaFirmwareInfo& firmware)
{
    ESP_LOGI(TAG, "Starting OTA update to version: %s", firmware.version.c_str());
    reportProgress(0, "Starting OTA download");

    const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (!updatePartition) {
        reportProgress(static_cast<int>(OtaProgressStep::FlashError), "No OTA partition available");
        return;
    }

    esp_ota_handle_t otaHandle = 0;
    if (esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle) != ESP_OK) {
        reportProgress(static_cast<int>(OtaProgressStep::FlashError), "OTA begin failed");
        return;
    }

    esp_http_client_config_t httpConfig = {};
    httpConfig.url               = firmware.url.c_str();
    httpConfig.timeout_ms        = 30000;
    httpConfig.buffer_size       = OTA_HTTP_BUFFER_SIZE;
    httpConfig.keep_alive_enable = true;
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t httpClient = esp_http_client_init(&httpConfig);
    if (!httpClient) {
        esp_ota_abort(otaHandle);
        reportProgress(static_cast<int>(OtaProgressStep::DownloadError), "HTTP client init failed");
        return;
    }

    if (esp_http_client_open(httpClient, 0) != ESP_OK) {
        esp_http_client_cleanup(httpClient);
        esp_ota_abort(otaHandle);
        reportProgress(static_cast<int>(OtaProgressStep::DownloadError), "HTTP connection failed");
        return;
    }

    int contentLength = esp_http_client_fetch_headers(httpClient);
    if (contentLength <= 0 && firmware.size > 0)
        contentLength = firmware.size;

    static char downloadBuffer[OTA_HTTP_BUFFER_SIZE];
    int totalReceived = 0;
    int lastPercent   = 0;

    mbedtls_md5_context md5Ctx;
    mbedtls_md5_init(&md5Ctx);
    mbedtls_md5_starts(&md5Ctx);

    while (true) {
        int bytesRead = esp_http_client_read(httpClient, downloadBuffer, OTA_HTTP_BUFFER_SIZE);
        if (bytesRead < 0) {
            mbedtls_md5_free(&md5Ctx);
            esp_http_client_cleanup(httpClient);
            esp_ota_abort(otaHandle);
            reportProgress(static_cast<int>(OtaProgressStep::DownloadError), "HTTP read error");
            return;
        }
        if (bytesRead == 0) break;

        mbedtls_md5_update(&md5Ctx, reinterpret_cast<const uint8_t*>(downloadBuffer), static_cast<size_t>(bytesRead));

        if (esp_ota_write(otaHandle, downloadBuffer, bytesRead) != ESP_OK) {
            mbedtls_md5_free(&md5Ctx);
            esp_http_client_cleanup(httpClient);
            esp_ota_abort(otaHandle);
            reportProgress(static_cast<int>(OtaProgressStep::FlashError), "Flash write failed");
            return;
        }

        totalReceived += bytesRead;
        if (contentLength > 0) {
            int pct = (totalReceived * 100) / contentLength;
            if (pct >= lastPercent + 10) {
                lastPercent = pct;
                reportProgress(pct, "Downloading firmware");
            }
        }
    }

    esp_http_client_cleanup(httpClient);
    ESP_LOGI(TAG, "Download complete, %d bytes", totalReceived);

    std::array<uint8_t, 16> digest{};
    mbedtls_md5_finish(&md5Ctx, digest.data());
    mbedtls_md5_free(&md5Ctx);

    if (!firmware.md5.empty() && !verifyMd5(digest, firmware.md5)) {
        esp_ota_abort(otaHandle);
        reportProgress(static_cast<int>(OtaProgressStep::VerifyError), "MD5 verification failed");
        return;
    }

    if (esp_ota_end(otaHandle) != ESP_OK) {
        reportProgress(static_cast<int>(OtaProgressStep::VerifyError), "OTA end failed");
        return;
    }

    if (esp_ota_set_boot_partition(updatePartition) != ESP_OK) {
        reportProgress(static_cast<int>(OtaProgressStep::FlashError), "Failed to set boot partition");
        return;
    }

    reportProgress(100, "OTA update complete, rebooting");
    ESP_LOGI(TAG, "OTA successful, restarting");
    esp_restart();
}

bool OtaModule::verifyMd5(const std::array<uint8_t, 16>& digest, const std::string& expectedMd5) const
{
    if (expectedMd5.size() != 32) return false;

    char computed[33] = {};
    for (int i = 0; i < 16; ++i)
        snprintf(&computed[i * 2], 3, "%02x", digest[i]);

    for (int i = 0; i < 32; ++i) {
        if (std::tolower(static_cast<unsigned char>(computed[i])) !=
            std::tolower(static_cast<unsigned char>(expectedMd5[i]))) {
            ESP_LOGE(TAG, "MD5 mismatch: computed=%s, expected=%s", computed, expectedMd5.c_str());
            return false;
        }
    }
    return true;
}

void OtaModule::reportProgress(int step, const std::string& description)
{
    std::string topic = buildOtaTopic("progress");

    cJSON* root = cJSON_CreateObject();
    if (!root) return;

    std::string msgId = std::to_string(MessageIdGenerator::generate());
    cJSON_AddStringToObject(root, "id", msgId.c_str());

    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "step", std::to_string(step).c_str());
    std::string desc = description.substr(0, 128);
    cJSON_AddStringToObject(params, "desc", desc.c_str());
    if (!moduleName_.empty())
        cJSON_AddStringToObject(params, "module", moduleName_.c_str());
    cJSON_AddItemToObject(root, "params", params);

    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!jsonStr) return;

    publish(topic, jsonStr);
    free(jsonStr);
    ESP_LOGI(TAG, "OTA progress: step=%d, desc=%s", step, description.c_str());
}

} // namespace alicloud::iot
