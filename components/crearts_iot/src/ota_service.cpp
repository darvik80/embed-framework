#include "crearts_iot/ota_service.hpp"

#include "embed/registry.hpp"
#include "embed/crypto.hpp"
#include "embed_core/firmware_slot.hpp"
#include "embed_core/mqtt_service.hpp"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cctype>
#include <cstring>

namespace crearts::iot {

static const char* TAG = "Ota";
static constexpr int kHttpBuf = 4096;
static constexpr uint32_t kTaskStack = 8192;
static constexpr UBaseType_t kTaskPrio = 5;

namespace {

struct TaskArg {
    OtaService* self = nullptr;
    OtaService::Firmware* fw = nullptr;
};

const char* jsonString(cJSON* obj, const char* key)
{
    cJSON* it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(it) && it->valuestring) return it->valuestring;
    return "";
}

bool jsonBool(cJSON* obj, const char* key, bool fallback = false)
{
    cJSON* it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(it)) return cJSON_IsTrue(it);
    if (cJSON_IsNumber(it)) return it->valuedouble != 0;
    return fallback;
}

} // namespace

void OtaService::start()
{
    iot_ = embed::ServiceRegistry::instance().getService<IotService>();
    if (!iot_) {
        ESP_LOGE(TAG, "CreartsIotService not found");
        return;
    }
    updateSlot_.connect(iot_->onOtaUpdate);
    cancelSlot_.connect(iot_->onOtaCancel);

    auto* mqtt = embed::ServiceRegistry::instance().getService<embed::MqttService>();
    if (mqtt) {
        mqttConnectedSlot_.connect(mqtt->onConnected);
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "OTA pending verify — valid after MQTT, else rollback in 90s");
        const esp_timer_create_args_t args = {
            .callback = &verifyTimeout,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ota_verify",
            .skip_unhandled_events = true
        };
        if (esp_timer_create(&args, &verifyTimer_) == ESP_OK) {
            esp_timer_start_once(verifyTimer_, 90ULL * 1000 * 1000);
        }
    }

    ESP_LOGI(TAG, "Listening for OTA update/cancel");
}

void OtaService::stop()
{
    cancel_.store(true);
    if (verifyTimer_) {
        esp_timer_stop(verifyTimer_);
        esp_timer_delete(verifyTimer_);
        verifyTimer_ = nullptr;
    }
    updateSlot_.disconnect();
    cancelSlot_.disconnect();
    mqttConnectedSlot_.disconnect();
    iot_ = nullptr;
}

void OtaService::confirmPendingImage()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) return;
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;

    ESP_LOGI(TAG, "OTA image pending verify — marking valid (MQTT up)");
    esp_ota_mark_app_valid_cancel_rollback();
    embed::noteFirmwareConfirmed();
    if (verifyTimer_) {
        esp_timer_stop(verifyTimer_);
        esp_timer_delete(verifyTimer_);
        verifyTimer_ = nullptr;
    }
}

void OtaService::onMqttConnected(const embed::MqttConnected&, void* ctx)
{
    static_cast<OtaService*>(ctx)->confirmPendingImage();
}

void OtaService::verifyTimeout(void* arg)
{
    auto* self = static_cast<OtaService*>(arg);
    ESP_LOGE(TAG, "MQTT not up in 90s after OTA — rolling back");
    if (self && self->verifyTimer_) {
        esp_timer_delete(self->verifyTimer_);
        self->verifyTimer_ = nullptr;
    }
    if (esp_ota_mark_app_invalid_rollback_and_reboot() == ESP_OK) {
        return;
    }
    if (embed::rollbackFirmware() == ESP_OK) {
        esp_restart();
    }
}

void OtaService::onUpdate(const OtaUpdate& msg, void* ctx)
{
    auto* self = static_cast<OtaService*>(ctx);
    self->handleUpdate(std::string_view(msg.payload.c_str(), msg.payload.size()));
}

void OtaService::onCancel(const OtaCancel& msg, void* ctx)
{
    auto* self = static_cast<OtaService*>(ctx);
    self->handleCancel(std::string_view(msg.payload.c_str(), msg.payload.size()));
}

bool OtaService::parseFirmware(std::string_view json, Firmware& out)
{
    if (json.empty()) return false;
    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    out.version = jsonString(root, "version");
    const char* module = jsonString(root, "module");
    out.module = (module && module[0]) ? module : "main";
    out.url = jsonString(root, "url");
    out.sha256 = jsonString(root, "sha256");
    out.stream = jsonString(root, "stream");
    out.force = jsonBool(root, "force", false);

    cJSON* size = cJSON_GetObjectItemCaseSensitive(root, "size");
    if (cJSON_IsNumber(size)) {
        out.size = static_cast<int32_t>(size->valuedouble);
    }

    cJSON_Delete(root);
    return !out.version.empty();
}

void OtaService::handleUpdate(std::string_view payload)
{
    ESP_LOGI(TAG, "OTA update payload (%u bytes): %.*s",
             static_cast<unsigned>(payload.size()),
             static_cast<int>(payload.size()), payload.data());

    Firmware fw;
    if (!parseFirmware(payload, fw)) {
        ESP_LOGE(TAG, "OTA update: invalid JSON / missing version");
        report("main", -1, "invalid OTA payload");
        return;
    }

    if (fw.stream == "mqtt" || fw.url.empty()) {
        ESP_LOGE(TAG, "OTA update: HTTPS url required (mqtt stream not supported)");
        report(fw.module.c_str(), -1, "url required");
        return;
    }

    const esp_app_desc_t* app = esp_app_get_description();
    const char* current = (app && app->version[0]) ? app->version : "";
    if (!fw.force && current[0] && fw.version == current) {
        ESP_LOGW(TAG, "OTA skipped — already on %s (force=false)", current);
        report(fw.module.c_str(), 100, "already on this version");
        return;
    }

    schedule(std::move(fw));
}

void OtaService::handleCancel(std::string_view payload)
{
    ESP_LOGW(TAG, "OTA cancel: %.*s", static_cast<int>(payload.size()), payload.data());
    if (!inProgress_.load()) return;
    cancel_.store(true);
}

void OtaService::schedule(Firmware fw)
{
    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
    if (!part) {
        ESP_LOGE(TAG, "No OTA partition — use partitions_ota.csv (ota_0/ota_1)");
        report(fw.module.c_str(), -3, "no OTA partition");
        return;
    }

    bool expected = false;
    if (!inProgress_.compare_exchange_strong(expected, true)) {
        ESP_LOGW(TAG, "OTA already in progress");
        report(fw.module.c_str(), -3, "ota busy");
        return;
    }
    cancel_.store(false);

    auto* arg = new TaskArg{this, new Firmware(std::move(fw))};
    if (xTaskCreate(otaTask, "crearts_ota", kTaskStack, arg, kTaskPrio, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        delete arg->fw;
        delete arg;
        inProgress_.store(false);
        report("main", -3, "failed to start ota task");
    }
}

void OtaService::otaTask(void* raw)
{
    auto* arg = static_cast<TaskArg*>(raw);
    OtaService* self = arg->self;
    Firmware* fw = arg->fw;
    delete arg;

    self->perform(*fw);
    delete fw;
    self->inProgress_.store(false);
    vTaskDelete(nullptr);
}

void OtaService::report(const char* module, int step, const char* desc)
{
    ESP_LOGI(TAG, "progress module=%s step=%d desc=%s", module ? module : "?", step, desc ? desc : "");
    if (iot_) {
        iot_->publishOtaProgress(module ? module : "main", step, desc ? desc : "", 1);
    }
}

bool OtaService::hexEqual(const uint8_t* digest, size_t digestLen, std::string_view hex)
{
    if (!digest || hex.size() != digestLen * 2) return false;
    for (size_t i = 0; i < digestLen; ++i) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        const int hi = nibble(hex[i * 2]);
        const int lo = nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        if (digest[i] != static_cast<uint8_t>((hi << 4) | lo)) return false;
    }
    return true;
}

void OtaService::perform(const Firmware& fw)
{
    ESP_LOGI(TAG, "Starting OTA %s module=%s url=%s size=%d",
             fw.version.c_str(), fw.module.c_str(), fw.url.c_str(), fw.size);
    report(fw.module.c_str(), 0, "Starting OTA download");

    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
    if (!part) {
        report(fw.module.c_str(), -3, "no OTA partition");
        return;
    }

    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
        report(fw.module.c_str(), -3, "ota begin failed");
        return;
    }

    esp_http_client_config_t httpCfg = {};
    httpCfg.url = fw.url.c_str();
    httpCfg.timeout_ms = 30000;
    httpCfg.buffer_size = kHttpBuf;
    httpCfg.keep_alive_enable = true;
    if (fw.url.rfind("https://", 0) == 0) {
        httpCfg.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t http = esp_http_client_init(&httpCfg);
    if (!http) {
        esp_ota_abort(ota);
        report(fw.module.c_str(), -1, "http client init failed");
        return;
    }
    if (esp_http_client_open(http, 0) != ESP_OK) {
        esp_http_client_cleanup(http);
        esp_ota_abort(ota);
        report(fw.module.c_str(), -1, "http connect failed");
        return;
    }

    int contentLength = esp_http_client_fetch_headers(http);
    if (contentLength <= 0 && fw.size > 0) {
        contentLength = fw.size;
    }

    embed::crypto::Sha256 sha;

    static char buf[kHttpBuf];
    int total = 0;
    int lastPct = 0;

    while (true) {
        if (cancel_.load()) {
            esp_http_client_cleanup(http);
            esp_ota_abort(ota);
            report(fw.module.c_str(), -4, "cancelled");
            return;
        }

        const int n = esp_http_client_read(http, buf, kHttpBuf);
        if (n < 0) {
            esp_http_client_cleanup(http);
            esp_ota_abort(ota);
            report(fw.module.c_str(), -1, "http read error");
            return;
        }
        if (n == 0) break;

        sha.update(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
        if (esp_ota_write(ota, buf, n) != ESP_OK) {
            esp_http_client_cleanup(http);
            esp_ota_abort(ota);
            report(fw.module.c_str(), -3, "flash write failed");
            return;
        }

        total += n;
        if (contentLength > 0) {
            const int pct = (total * 100) / contentLength;
            if (pct >= lastPct + 10) {
                lastPct = pct > 99 ? 99 : pct;
                report(fw.module.c_str(), lastPct, "Downloading firmware");
            }
        }
    }

    esp_http_client_cleanup(http);
    ESP_LOGI(TAG, "Download complete, %d bytes", total);

    uint8_t digest[32]{};
    if (!sha.finish(digest)) {
        esp_ota_abort(ota);
        report(fw.module.c_str(), -2, "sha256 failed");
        return;
    }

    if (!fw.sha256.empty() && !hexEqual(digest, sizeof(digest), fw.sha256)) {
        esp_ota_abort(ota);
        report(fw.module.c_str(), -2, "sha256 mismatch");
        return;
    }

    if (esp_ota_end(ota) != ESP_OK) {
        report(fw.module.c_str(), -2, "ota end failed");
        return;
    }
    if (esp_ota_set_boot_partition(part) != ESP_OK) {
        report(fw.module.c_str(), -3, "set boot partition failed");
        return;
    }

    report(fw.module.c_str(), 101, "OTA complete, rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

} // namespace crearts::iot
