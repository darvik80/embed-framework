#include "cogitor_iot/settings.hpp"

#include "embed_core/nvs_store.hpp"

#include "cJSON.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace cogitor::iot {

static const char* TAG = "CogitorSettings";

namespace {

constexpr char kNs[] = "cogitor";
constexpr char kBakNs[] = "cogitor_b";

esp_err_t eraseNamespace(const char* ns)
{
    embed::NvsStore store;
    esp_err_t err = store.open(ns);
    if (err != ESP_OK) {
        return err;
    }
    err = store.eraseAll();
    if (err == ESP_OK) {
        err = store.commit();
    }
    return err;
}

bool loadFrom(const char* ns, CogitorSettings& out)
{
    out = {};
    embed::NvsStore store;
    if (store.open(ns) != ESP_OK) {
        return false;
    }
    store.getString("product", out.product, sizeof(out.product));
    store.getString("device", out.device, sizeof(out.device));
    store.getString("host", out.host, sizeof(out.host));
    store.getString("token", out.token, sizeof(out.token));
    store.getU16("port", out.port);
    uint8_t tls = 0;
    uint8_t topicShort = 0;
    store.getU8("tls", tls);
    store.getU8("short", topicShort);
    out.useTls = tls != 0;
    out.topicShort = topicShort != 0;
    return out.product[0] || out.device[0] || out.host[0] || out.token[0];
}

esp_err_t saveTo(const char* ns, const CogitorSettings& in)
{
    embed::NvsStore store;
    esp_err_t err = store.open(ns);
    if (err != ESP_OK) {
        return err;
    }
    err = store.setString("product", in.product);
    if (err == ESP_OK) err = store.setString("device", in.device);
    if (err == ESP_OK) err = store.setString("host", in.host);
    if (err == ESP_OK) err = store.setString("token", in.token);
    if (err == ESP_OK) err = store.setU16("port", in.port);
    if (err == ESP_OK) err = store.setU8("tls", in.useTls ? 1 : 0);
    if (err == ESP_OK) err = store.setU8("short", in.topicShort ? 1 : 0);
    if (err == ESP_OK) err = store.commit();
    return err;
}

cJSON* jsonChild(cJSON* obj, std::initializer_list<const char*> keys)
{
    if (!obj) {
        return nullptr;
    }
    for (const char* key : keys) {
        cJSON* it = cJSON_GetObjectItemCaseSensitive(obj, key);
        if (it) {
            return it;
        }
    }
    return nullptr;
}

void copyJsonString(cJSON* obj, std::initializer_list<const char*> keys, char* dst, size_t dstLen)
{
    cJSON* it = jsonChild(obj, keys);
    if (!cJSON_IsString(it) || !it->valuestring || !it->valuestring[0] || !dst || dstLen == 0) {
        return;
    }
    std::strncpy(dst, it->valuestring, dstLen - 1);
    dst[dstLen - 1] = '\0';
}

bool copyJsonBool(cJSON* obj, std::initializer_list<const char*> keys, bool& out)
{
    cJSON* it = jsonChild(obj, keys);
    if (!it) {
        return false;
    }
    if (cJSON_IsBool(it)) {
        out = cJSON_IsTrue(it);
        return true;
    }
    if (cJSON_IsNumber(it)) {
        out = it->valuedouble != 0;
        return true;
    }
    if (cJSON_IsString(it) && it->valuestring) {
        out = std::strcmp(it->valuestring, "1") == 0 ||
              std::strcmp(it->valuestring, "true") == 0 ||
              std::strcmp(it->valuestring, "yes") == 0;
        return true;
    }
    return false;
}

bool copyJsonPort(cJSON* obj, CogitorSettings& s)
{
    cJSON* it = jsonChild(obj, {"port"});
    if (!it) {
        return false;
    }
    if (cJSON_IsNumber(it)) {
        const int v = static_cast<int>(it->valuedouble);
        s.port = v < 0 ? 0 : (v > 65535 ? 65535 : static_cast<uint16_t>(v));
        return true;
    }
    if (cJSON_IsString(it) && it->valuestring && it->valuestring[0]) {
        const int v = std::atoi(it->valuestring);
        s.port = v < 0 ? 0 : (v > 65535 ? 65535 : static_cast<uint16_t>(v));
        return true;
    }
    return false;
}

void overlayWifiJson(cJSON* obj, embed::WifiSettings& wifi)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    copyJsonString(obj, {"ssid"}, wifi.ssid, sizeof(wifi.ssid));
    copyJsonString(obj, {"password", "pass"}, wifi.password, sizeof(wifi.password));
}

void overlayCogitorJson(cJSON* obj, CogitorSettings& s)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    copyJsonString(obj, {"product", "product_id", "productId"}, s.product, sizeof(s.product));
    copyJsonString(obj, {"device", "device_id", "deviceId"}, s.device, sizeof(s.device));
    copyJsonString(obj, {"host", "broker", "broker_host"}, s.host, sizeof(s.host));
    copyJsonString(obj, {"token", "access_token", "accessToken"}, s.token, sizeof(s.token));
    copyJsonPort(obj, s);
    copyJsonBool(obj, {"tls", "use_tls", "useTls"}, s.useTls);
    copyJsonBool(obj, {"topic_short", "topicShort", "short"}, s.topicShort);
}

} // namespace

bool loadSettings(CogitorSettings& out)
{
    return loadFrom(kNs, out);
}

esp_err_t saveSettings(const CogitorSettings& in)
{
    return saveTo(kNs, in);
}

bool settingsComplete(const CogitorSettings& s)
{
    return s.product[0] && s.device[0] && s.host[0] && s.token[0];
}

bool loadSettingsBackup(CogitorSettings& out)
{
    return loadFrom(kBakNs, out);
}

bool deviceSettingsBackupPresent()
{
    embed::WifiSettings w{};
    CogitorSettings c{};
    return embed::loadWifiBackup(w) || loadSettingsBackup(c);
}

esp_err_t backupDeviceSettings()
{
    embed::WifiSettings w{};
    CogitorSettings c{};
    const bool haveW = embed::loadWifiSettings(w);
    const bool haveC = loadSettings(c);
    if (!haveW && !haveC) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err = ESP_OK;
    if (haveW) {
        err = embed::saveWifiBackup(w);
    }
    if (err == ESP_OK && haveC) {
        err = saveTo(kBakNs, c);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "backup ssid=%s %s.%s @ %s",
                 haveW ? w.ssid : "-",
                 haveC ? c.product : "-",
                 haveC ? c.device : "-",
                 haveC ? c.host : "-");
    }
    return err;
}

esp_err_t restoreDeviceSettingsBackup()
{
    embed::WifiSettings bakW{};
    CogitorSettings bakC{};
    const bool haveW = embed::loadWifiBackup(bakW);
    const bool haveC = loadSettingsBackup(bakC);
    if (!haveW && !haveC) {
        return ESP_ERR_NOT_FOUND;
    }

    embed::WifiSettings curW{};
    CogitorSettings curC{};
    const bool hadW = embed::loadWifiSettings(curW);
    const bool hadC = loadSettings(curC);

    esp_err_t err = ESP_OK;
    if (haveW) {
        err = embed::saveWifiSettings(bakW);
    }
    if (err == ESP_OK && haveC) {
        err = saveSettings(bakC);
    }
    if (err == ESP_OK && (hadW || hadC)) {
        if (hadW) {
            err = embed::saveWifiBackup(curW);
        }
        if (err == ESP_OK && hadC) {
            err = saveTo(kBakNs, curC);
        }
    }
    if (err == ESP_OK) {
        err = embed::setConfigPortalRequested(false);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "restored ssid=%s %s.%s @ %s",
                 haveW ? bakW.ssid : "-",
                 haveC ? bakC.product : "-",
                 haveC ? bakC.device : "-",
                 haveC ? bakC.host : "-");
    }
    return err;
}

esp_err_t parseCredentialsJson(const char* json, embed::WifiSettings& wifi, CogitorSettings& cogitor)
{
    if (!json || !json[0]) {
        ESP_LOGE(TAG, "credentials JSON empty");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        ESP_LOGE(TAG, "credentials JSON is not an object");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* wifiObj = jsonChild(root, {"wifi", "WiFi"});
    cJSON* cogitorObj = jsonChild(root, {"cogitor", "mqtt"});
    overlayWifiJson(wifiObj ? wifiObj : root, wifi);
    overlayCogitorJson(cogitorObj ? cogitorObj : root, cogitor);
    cJSON_Delete(root);

    if (!wifi.ssid[0]) {
        ESP_LOGE(TAG, "credentials JSON missing wifi.ssid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!settingsComplete(cogitor)) {
        ESP_LOGE(TAG, "credentials JSON missing cogitor product/device/host/token");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t importCredentialsJson(const char* json)
{
    embed::WifiSettings wifi{};
    CogitorSettings cogitor{};
    if (!embed::loadWifiSettings(wifi)) {
        ESP_LOGW(TAG, "no wifi settings found");
    }
    if (!loadSettings(cogitor)) {
        ESP_LOGW(TAG, "no cogitor settings found");
    }

    const esp_err_t parsed = parseCredentialsJson(json, wifi, cogitor);
    if (parsed != ESP_OK) {
        return parsed;
    }

    const esp_err_t bak = backupDeviceSettings();
    if (bak != ESP_OK && bak != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "backup before JSON import failed: %s", esp_err_to_name(bak));
    }

    esp_err_t err = embed::saveWifiSettings(wifi);
    if (err == ESP_OK) {
        err = saveSettings(cogitor);
    }
    if (err == ESP_OK) {
        err = embed::setConfigPortalRequested(false);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "imported wifi=%s cogitor=%s.%s @ %s",
                 wifi.ssid, cogitor.product, cogitor.device, cogitor.host);
    }
    return err;
}

esp_err_t exportCredentialsJson(char* out, size_t outLen, bool includeSecrets)
{
    if (!out || outLen == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    embed::WifiSettings wifi{};
    CogitorSettings cogitor{};
    if (!embed::loadWifiSettings(wifi)) {
        ESP_LOGW(TAG, "no wifi settings found");
    }
    if (!loadSettings(cogitor)) {
        ESP_LOGW(TAG, "no cogitor settings found");
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON* w = cJSON_AddObjectToObject(root, "wifi");
    cJSON* c = cJSON_AddObjectToObject(root, "cogitor");
    if (!w || !c) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(w, "ssid", wifi.ssid);
    cJSON_AddStringToObject(w, "password", includeSecrets ? wifi.password : "");
    cJSON_AddStringToObject(c, "product", cogitor.product);
    cJSON_AddStringToObject(c, "device", cogitor.device);
    cJSON_AddStringToObject(c, "host", cogitor.host);
    cJSON_AddNumberToObject(c, "port", cogitor.port);
    cJSON_AddStringToObject(c, "token", includeSecrets ? cogitor.token : "");
    cJSON_AddBoolToObject(c, "tls", cogitor.useTls);
    cJSON_AddBoolToObject(c, "topic_short", cogitor.topicShort);

    char* printed = cJSON_Print(root);
    cJSON_Delete(root);
    if (!printed) {
        return ESP_ERR_NO_MEM;
    }
    const size_t n = std::strlen(printed);
    if (n + 1 > outLen) {
        free(printed);
        return ESP_ERR_INVALID_SIZE;
    }
    std::memcpy(out, printed, n + 1);
    free(printed);
    return ESP_OK;
}

esp_err_t factoryResetSettings()
{
    ESP_LOGW(TAG, "factory reset wifi + cogitor");
    const esp_err_t bak = backupDeviceSettings();
    if (bak != ESP_OK && bak != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "backup before reset failed: %s", esp_err_to_name(bak));
    }
    embed::eraseWifiSettings();
    eraseNamespace(kNs);
    return embed::setConfigPortalRequested(true);
}

void installFactoryResetHandler()
{
    embed::setFactoryResetHandler(&factoryResetSettings);
}

} // namespace cogitor::iot
