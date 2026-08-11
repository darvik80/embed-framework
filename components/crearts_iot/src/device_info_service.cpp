#include "crearts_iot/device_info_service.hpp"

#include "embed/registry.hpp"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"

#include <cstdio>
#include <string_view>

namespace crearts::iot {

static const char* TAG = "Info";

/// Product-facing reported attrs (esp_app_desc fills version / project name).
static constexpr char kAppModel[] = "ESP32-S3";
static constexpr char kAppName[] = "embed-framework";
static constexpr char kAppVendor[] = "crearts";
static constexpr char kAppProtocol[] = "iot/v1";

static const char* chipModelName(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32: return "ESP32";
    case CHIP_ESP32S2: return "ESP32-S2";
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32C3: return "ESP32-C3";
#ifdef CHIP_ESP32C2
    case CHIP_ESP32C2: return "ESP32-C2";
#endif
#ifdef CHIP_ESP32C6
    case CHIP_ESP32C6: return "ESP32-C6";
#endif
#ifdef CHIP_ESP32H2
    case CHIP_ESP32H2: return "ESP32-H2";
#endif
#ifdef CHIP_ESP32P4
    case CHIP_ESP32P4: return "ESP32-P4";
#endif
    default: return CONFIG_IDF_TARGET;
    }
}

void DeviceInfo::start()
{
    auto& reg = embed::ServiceRegistry::instance();
    iot_ = reg.getService<IotService>();
    mqtt_ = reg.getService<embed::MqttService>();
    if (!iot_ || !mqtt_) {
        ESP_LOGE(TAG, "CreartsIotService/MqttService missing");
        return;
    }
    connectedSlot_.connect(mqtt_->onConnected);
    attrUpdateSlot_.connect(iot_->onAttributeUpdate);
    attrResponseSlot_.connect(iot_->onAttributeResponse);
    ESP_LOGI(TAG, "Will report/request attributes on MQTT connect");
}

void DeviceInfo::stop()
{
    connectedSlot_.disconnect();
    attrUpdateSlot_.disconnect();
    attrResponseSlot_.disconnect();
    iot_ = nullptr;
    mqtt_ = nullptr;
}

void DeviceInfo::publishReported(IotService* iot)
{
    const esp_app_desc_t* app = esp_app_get_description();
    const char* version = (app && app->version[0]) ? app->version : "0.0.0";
    const char* appName = (app && app->project_name[0]) ? app->project_name : kAppName;
    const char* idfVer = (app && app->idf_ver[0]) ? app->idf_ver : "";

    esp_chip_info_t chip{};
    esp_chip_info(&chip);

    uint8_t mac[6]{};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    std::snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    AttributeBuilder attrs;
    attrs.add("version", version)
        .add("model", kAppModel)
        .add("appName", appName)
        .add("vendor", kAppVendor)
        .add("protocol", kAppProtocol)
        .add("productId", iot->credentials().productId())
        .add("deviceId", iot->credentials().deviceId())
        .add("chip", chipModelName(chip.model))
        .add("chipCores", static_cast<int>(chip.cores))
        .add("idfVersion", idfVer)
        .add("mac", macStr);

    const int msgId = iot->publishAttributes(attrs, 1);
    ESP_LOGI(TAG, "reported attrs msg_id=%d version=%s model=%s app=%s",
             msgId, version, kAppModel, appName);
    iot->publishOtaVersion(version, "main", 1);
}

void DeviceInfo::onConnected(const embed::MqttConnected&, void* ctx)
{
    auto* self = static_cast<DeviceInfo*>(ctx);
    if (!self->iot_) {
        return;
    }

    publishReported(self->iot_);

    AttributeRequestBuilder req;
    req.desiredAll();
    uint32_t reqId = 0;
    const int msgId = self->iot_->requestAttributes(req, reqId, 1);
    ESP_LOGI(TAG, "desired request msg_id=%d id=%lu",
             msgId, static_cast<unsigned long>(reqId));
}

void DeviceInfo::onAttrUpdate(const AttributeUpdate& upd, void* /*ctx*/)
{
    auto parsed = parseAttributeUpdate(
        std::string_view(upd.payload.c_str(), upd.payload.size()));
    ESP_LOGI(TAG, "desired update: %s", parsed.desiredJson.c_str());
}

void DeviceInfo::onAttrResponse(const AttributeResponse& res, void* /*ctx*/)
{
    auto parsed = parseAttributeResponse(
        std::string_view(res.payload.c_str(), res.payload.size()));
    ESP_LOGI(TAG, "attr response id=%lu reported=%s desired=%s",
             static_cast<unsigned long>(res.requestId),
             parsed.reportedJson.c_str(),
             parsed.desiredJson.c_str());
}

} // namespace crearts::iot
