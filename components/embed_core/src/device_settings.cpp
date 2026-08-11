#include "embed_core/device_settings.hpp"
#include "embed_core/nvs_store.hpp"

#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace embed {

static const char* TAG = "DevSettings";

namespace {

constexpr char kWifiNs[] = "wifi";
constexpr char kCreartsNs[] = "crearts";
constexpr char kWifiBakNs[] = "wifi_b";
constexpr char kCreartsBakNs[] = "crearts_b";
constexpr char kEmbedNs[] = "embed";
constexpr char kPortalKey[] = "portal";

esp_err_t eraseNamespace(const char* ns)
{
    NvsStore store;
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

void rebootTask(void* arg)
{
    const auto delayMs = reinterpret_cast<uintptr_t>(arg);
    if (delayMs > 0) {
        vTaskDelay(pdMS_TO_TICKS(delayMs));
    }
    ESP_LOGW(TAG, "Rebooting now");
    esp_restart();
}

} // namespace

static bool loadWifiFrom(const char* ns, WifiSettings& out)
{
    out = {};
    NvsStore store;
    if (store.open(ns) != ESP_OK) {
        return false;
    }
    const bool have = store.getString("ssid", out.ssid, sizeof(out.ssid)) && out.ssid[0];
    store.getString("pass", out.password, sizeof(out.password));
    return have;
}

static esp_err_t saveWifiTo(const char* ns, const WifiSettings& in)
{
    NvsStore store;
    esp_err_t err = store.open(ns);
    if (err != ESP_OK) {
        return err;
    }
    err = store.setString("ssid", in.ssid);
    if (err == ESP_OK) {
        err = store.setString("pass", in.password);
    }
    if (err == ESP_OK) {
        err = store.commit();
    }
    return err;
}

static bool loadCreartsFrom(const char* ns, CreartsSettings& out)
{
    out = {};
    NvsStore store;
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

static esp_err_t saveCreartsTo(const char* ns, const CreartsSettings& in)
{
    NvsStore store;
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

bool loadWifiSettings(WifiSettings& out)
{
    return loadWifiFrom(kWifiNs, out);
}

esp_err_t saveWifiSettings(const WifiSettings& in)
{
    return saveWifiTo(kWifiNs, in);
}

bool wifiSettingsPresent()
{
    WifiSettings w{};
    return loadWifiSettings(w);
}

bool loadCreartsSettings(CreartsSettings& out)
{
    return loadCreartsFrom(kCreartsNs, out);
}

esp_err_t saveCreartsSettings(const CreartsSettings& in)
{
    return saveCreartsTo(kCreartsNs, in);
}

bool loadWifiBackup(WifiSettings& out)
{
    return loadWifiFrom(kWifiBakNs, out);
}

bool loadCreartsBackup(CreartsSettings& out)
{
    return loadCreartsFrom(kCreartsBakNs, out);
}

bool settingsBackupPresent()
{
    WifiSettings w{};
    CreartsSettings c{};
    return loadWifiBackup(w) || loadCreartsBackup(c);
}

esp_err_t backupSettings()
{
    WifiSettings w{};
    CreartsSettings c{};
    const bool haveW = loadWifiSettings(w);
    const bool haveC = loadCreartsSettings(c);
    if (!haveW && !haveC) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err = ESP_OK;
    if (haveW) {
        err = saveWifiTo(kWifiBakNs, w);
    }
    if (err == ESP_OK && haveC) {
        err = saveCreartsTo(kCreartsBakNs, c);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "settings backup saved ssid=%s %s.%s @ %s",
                 haveW ? w.ssid : "-",
                 haveC ? c.product : "-",
                 haveC ? c.device : "-",
                 haveC ? c.host : "-");
    }
    return err;
}

esp_err_t restoreSettingsBackup()
{
    WifiSettings bakW{};
    CreartsSettings bakC{};
    const bool haveW = loadWifiBackup(bakW);
    const bool haveC = loadCreartsBackup(bakC);
    if (!haveW && !haveC) {
        return ESP_ERR_NOT_FOUND;
    }

    WifiSettings curW{};
    CreartsSettings curC{};
    const bool hadW = loadWifiSettings(curW);
    const bool hadC = loadCreartsSettings(curC);

    esp_err_t err = ESP_OK;
    if (haveW) {
        err = saveWifiSettings(bakW);
    }
    if (err == ESP_OK && haveC) {
        err = saveCreartsSettings(bakC);
    }
    if (err == ESP_OK && (hadW || hadC)) {
        if (hadW) {
            err = saveWifiTo(kWifiBakNs, curW);
        }
        if (err == ESP_OK && hadC) {
            err = saveCreartsTo(kCreartsBakNs, curC);
        }
    }
    if (err == ESP_OK) {
        err = setConfigPortalRequested(false);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "settings restored from backup ssid=%s %s.%s @ %s",
                 haveW ? bakW.ssid : "-",
                 haveC ? bakC.product : "-",
                 haveC ? bakC.device : "-",
                 haveC ? bakC.host : "-");
    }
    return err;
}

namespace {

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

bool copyJsonPort(cJSON* obj, CreartsSettings& crearts)
{
    cJSON* it = jsonChild(obj, {"port"});
    if (!it) {
        return false;
    }
    if (cJSON_IsNumber(it)) {
        const int v = static_cast<int>(it->valuedouble);
        crearts.port = v < 0 ? 0 : (v > 65535 ? 65535 : static_cast<uint16_t>(v));
        return true;
    }
    if (cJSON_IsString(it) && it->valuestring && it->valuestring[0]) {
        const int v = std::atoi(it->valuestring);
        crearts.port = v < 0 ? 0 : (v > 65535 ? 65535 : static_cast<uint16_t>(v));
        return true;
    }
    return false;
}

void overlayWifiJson(cJSON* obj, WifiSettings& wifi)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    copyJsonString(obj, {"ssid"}, wifi.ssid, sizeof(wifi.ssid));
    copyJsonString(obj, {"password", "pass"}, wifi.password, sizeof(wifi.password));
}

void overlayCreartsJson(cJSON* obj, CreartsSettings& crearts)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }
    copyJsonString(obj, {"product", "product_id", "productId"}, crearts.product,
                   sizeof(crearts.product));
    copyJsonString(obj, {"device", "device_id", "deviceId"}, crearts.device, sizeof(crearts.device));
    copyJsonString(obj, {"host", "broker", "broker_host"}, crearts.host, sizeof(crearts.host));
    copyJsonString(obj, {"token", "access_token", "accessToken"}, crearts.token,
                   sizeof(crearts.token));
    copyJsonPort(obj, crearts);
    copyJsonBool(obj, {"tls", "use_tls", "useTls"}, crearts.useTls);
    copyJsonBool(obj, {"topic_short", "topicShort", "short"}, crearts.topicShort);
}

} // namespace

esp_err_t parseCredentialsJson(const char* json, WifiSettings& wifi, CreartsSettings& crearts)
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
    cJSON* creartsObj = jsonChild(root, {"crearts", "mqtt", "crearts_iot"});
    overlayWifiJson(wifiObj ? wifiObj : root, wifi);
    overlayCreartsJson(creartsObj ? creartsObj : root, crearts);
    cJSON_Delete(root);

    if (!wifi.ssid[0]) {
        ESP_LOGE(TAG, "credentials JSON missing wifi.ssid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!creartsSettingsComplete(crearts)) {
        ESP_LOGE(TAG, "credentials JSON missing crearts product/device/host/token");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t importCredentialsJson(const char* json)
{
    WifiSettings wifi{};
    CreartsSettings crearts{};
    if (!loadWifiSettings(wifi))
    {
        ESP_LOGW(TAG, "no wifi settings found");
    }
    if (loadCreartsSettings(crearts)) {
        ESP_LOGW(TAG, "no crearts settings found");
    }

    const esp_err_t parsed = parseCredentialsJson(json, wifi, crearts);
    if (parsed != ESP_OK) {
        return parsed;
    }

    const esp_err_t bak = backupSettings();
    if (bak != ESP_OK && bak != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "backup before JSON import failed: %s", esp_err_to_name(bak));
    }

    esp_err_t err = saveWifiSettings(wifi);
    if (err == ESP_OK) {
        err = saveCreartsSettings(crearts);
    }
    if (err == ESP_OK) {
        err = setConfigPortalRequested(false);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "imported credentials wifi=%s crearts=%s.%s @ %s",
                 wifi.ssid, crearts.product, crearts.device, crearts.host);
    }
    return err;
}

esp_err_t exportCredentialsJson(char* out, size_t outLen, bool includeSecrets)
{
    if (!out || outLen == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    WifiSettings wifi{};
    CreartsSettings crearts{};
    if (!loadWifiSettings(wifi))
    {
        ESP_LOGW(TAG, "no wifi settings found");
    }
    if (loadCreartsSettings(crearts)) {
        ESP_LOGW(TAG, "no crearts settings found");
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON* w = cJSON_AddObjectToObject(root, "wifi");
    cJSON* c = cJSON_AddObjectToObject(root, "crearts");
    if (!w || !c) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(w, "ssid", wifi.ssid);
    cJSON_AddStringToObject(w, "password", includeSecrets ? wifi.password : "");
    cJSON_AddStringToObject(c, "product", crearts.product);
    cJSON_AddStringToObject(c, "device", crearts.device);
    cJSON_AddStringToObject(c, "host", crearts.host);
    cJSON_AddNumberToObject(c, "port", crearts.port);
    cJSON_AddStringToObject(c, "token", includeSecrets ? crearts.token : "");
    cJSON_AddBoolToObject(c, "tls", crearts.useTls);
    cJSON_AddBoolToObject(c, "topic_short", crearts.topicShort);

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

bool creartsSettingsComplete(const CreartsSettings& s)
{
    return s.product[0] && s.device[0] && s.host[0] && s.token[0];
}

bool isConfigPortalRequested()
{
    NvsStore store;
    if (store.open(kEmbedNs) != ESP_OK) {
        return false;
    }
    uint8_t flag = 0;
    return store.getU8(kPortalKey, flag) && flag != 0;
}

esp_err_t setConfigPortalRequested(bool on)
{
    NvsStore store;
    esp_err_t err = store.open(kEmbedNs);
    if (err != ESP_OK) {
        return err;
    }
    if (on) {
        err = store.setU8(kPortalKey, 1);
    } else {
        err = store.erase(kPortalKey);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = store.commit();
    }
    return err;
}

bool needsConfigPortal()
{
    if (isConfigPortalRequested()) {
        return true;
    }
    if (wifiSettingsPresent()) {
        return false;
    }
#ifdef CONFIG_EMBED_WIFI_SSID
    if (CONFIG_EMBED_WIFI_SSID[0] != '\0') {
        return false;
    }
#endif
    return true;
}

esp_err_t factoryResetSettings()
{
    ESP_LOGW(TAG, "factory reset settings");
    const esp_err_t bak = backupSettings();
    if (bak != ESP_OK && bak != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "backup before reset failed: %s", esp_err_to_name(bak));
    }
    // Wipe active creds only — keep wifi_b / crearts_b for rollback.
    eraseNamespace(kWifiNs);
    eraseNamespace(kCreartsNs);
    return setConfigPortalRequested(true);
}

esp_err_t requestConfigPortal()
{
    ESP_LOGW(TAG, "config portal requested (keep creds)");
    return setConfigPortalRequested(true);
}

#if defined(CONFIG_EMBED_CONFIG_RESET_GPIO)
static constexpr int kResetGpio = CONFIG_EMBED_CONFIG_RESET_GPIO;
#else
static constexpr int kResetGpio = 0; // BOOT on ESP32-S3
#endif

#if defined(CONFIG_EMBED_CONFIG_RESET_HOLD_MS)
static constexpr int kHoldMs = CONFIG_EMBED_CONFIG_RESET_HOLD_MS;
#else
static constexpr int kHoldMs = 3000;
#endif

static bool resetGpioReady = false;

static bool initResetGpio()
{
    if (kResetGpio < 0) {
        return false;
    }
    if (resetGpioReady) {
        return true;
    }
    const gpio_num_t pin = static_cast<gpio_num_t>(kResetGpio);
    gpio_reset_pin(pin);
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << static_cast<unsigned>(kResetGpio);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "reset GPIO %d config failed", kResetGpio);
        return false;
    }
    resetGpioReady = true;
    return true;
}

static bool resetGpioLow()
{
    return gpio_get_level(static_cast<gpio_num_t>(kResetGpio)) == 0;
}

bool factoryResetGpioHeld()
{
    if (!initResetGpio()) {
        return false;
    }
    ESP_LOGI(TAG, "reset GPIO %d level=%d (0=pressed) hold=%d ms",
             kResetGpio, resetGpioLow() ? 0 : 1, kHoldMs);
    if (!resetGpioLow()) {
        return false;
    }
    ESP_LOGW(TAG, "reset GPIO %d low — hold %d ms to confirm", kResetGpio, kHoldMs);
    const TickType_t t0 = xTaskGetTickCount();
    while ((xTaskGetTickCount() - t0) < pdMS_TO_TICKS(kHoldMs)) {
        if (!resetGpioLow()) {
            ESP_LOGI(TAG, "reset GPIO released — cancel");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return resetGpioLow();
}

static void gpioWatchTask(void* /*arg*/)
{
    ESP_LOGI(TAG, "hold GPIO %d (BOOT) %d ms anytime → factory reset",
             kResetGpio, kHoldMs);
    bool holding = false;
    TickType_t since = 0;
    for (;;) {
        if (resetGpioLow()) {
            if (!holding) {
                holding = true;
                since = xTaskGetTickCount();
                ESP_LOGW(TAG, "BOOT pressed — keep holding %d ms to wipe creds", kHoldMs);
            } else if ((xTaskGetTickCount() - since) >= pdMS_TO_TICKS(kHoldMs)) {
                ESP_LOGW(TAG, "BOOT held — factory reset + portal");
                if (factoryResetSettings() == ESP_OK) {
                    scheduleReboot(300);
                }
                vTaskDelete(nullptr);
                return;
            }
        } else if (holding) {
            holding = false;
            ESP_LOGI(TAG, "BOOT released — factory reset cancelled");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void startFactoryResetGpioWatch()
{
    if (!initResetGpio()) {
        return;
    }
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    xTaskCreate(gpioWatchTask, "rst_gpio", 4096, nullptr, 4, nullptr);
}

void scheduleReboot(uint32_t delayMs)
{
    xTaskCreate(rebootTask, "reboot", 2048, reinterpret_cast<void*>(delayMs), 5, nullptr);
}

#if defined(CONFIG_EMBED_CONFIG_RST_BURST_COUNT)
static constexpr uint32_t kRstBurstNeed = CONFIG_EMBED_CONFIG_RST_BURST_COUNT;
#else
static constexpr uint32_t kRstBurstNeed = 3;
#endif

#if defined(CONFIG_EMBED_CONFIG_RST_BURST_CLEAR_MS)
static constexpr uint32_t kRstBurstClearMs = CONFIG_EMBED_CONFIG_RST_BURST_CLEAR_MS;
#else
static constexpr uint32_t kRstBurstClearMs = 10000;
#endif

static constexpr uint32_t kRstBurstMagic = 0x52535421; // 'RST!'
static constexpr char kRstCountKey[] = "rstc";

struct RstBurst {
    uint32_t magic;
    uint32_t count;
};

RTC_NOINIT_ATTR static RstBurst s_rstBurst;
static esp_timer_handle_t s_rstClearTimer = nullptr;

static const char* resetReasonName(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    default: return "OTHER";
    }
}

/// ESP32-S3 EN/RST often reports POWERON (not EXT). RTC RAM is also wiped
/// on that path — persist the counter in NVS (`fctry` / `embed.rstc`).
static bool isUserRstButton(esp_reset_reason_t r)
{
    return r == ESP_RST_EXT || r == ESP_RST_USB || r == ESP_RST_POWERON;
}

static uint32_t loadNvsBurstCount()
{
    NvsStore store;
    if (store.open(kEmbedNs) != ESP_OK) {
        return 0;
    }
    uint8_t c = 0;
    return store.getU8(kRstCountKey, c) ? c : 0;
}

static void saveNvsBurstCount(uint32_t count)
{
    NvsStore store;
    if (store.open(kEmbedNs) != ESP_OK) {
        return;
    }
    if (count == 0) {
        if (store.erase(kRstCountKey) == ESP_ERR_NVS_NOT_FOUND) {
            return;
        }
    } else {
        const uint8_t c = count > 255 ? 255 : static_cast<uint8_t>(count);
        if (store.setU8(kRstCountKey, c) != ESP_OK) {
            return;
        }
    }
    store.commit();
}

static uint32_t readBurstCount()
{
    if (s_rstBurst.magic == kRstBurstMagic) {
        return s_rstBurst.count;
    }
    return loadNvsBurstCount();
}

static void writeBurstCount(uint32_t count)
{
    s_rstBurst.magic = kRstBurstMagic;
    s_rstBurst.count = count;
    saveNvsBurstCount(count);
}

static void rstBurstClearCb(void* /*arg*/)
{
    const uint32_t was = readBurstCount();
    if (was == 0) {
        return;
    }
    ESP_LOGI(TAG, "EN/RST burst counter cleared (was %lu)",
             static_cast<unsigned long>(was));
    writeBurstCount(0);
}

bool checkRstBurstFactoryReset()
{
    if (kRstBurstNeed == 0) {
        return false;
    }

    const esp_reset_reason_t why = esp_reset_reason();
    uint32_t count = readBurstCount();

    if (isUserRstButton(why)) {
        if (count < 100) {
            ++count;
        }
        writeBurstCount(count);
    }

    ESP_LOGI(TAG, "reset=%s EN/RST burst=%lu/%lu",
             resetReasonName(why),
             static_cast<unsigned long>(count),
             static_cast<unsigned long>(kRstBurstNeed));

    if (count >= kRstBurstNeed) {
        ESP_LOGW(TAG, "EN/RST x%lu — factory reset + portal",
                 static_cast<unsigned long>(count));
        writeBurstCount(0);
        return true;
    }

    if (count > 0) {
        if (!s_rstClearTimer) {
            const esp_timer_create_args_t args = {
                .callback = &rstBurstClearCb,
                .arg = nullptr,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "rst_burst",
                .skip_unhandled_events = true
            };
            if (esp_timer_create(&args, &s_rstClearTimer) != ESP_OK) {
                ESP_LOGW(TAG, "EN/RST clear timer create failed");
                return false;
            }
        }
        esp_timer_stop(s_rstClearTimer);
        esp_timer_start_once(s_rstClearTimer,
                             static_cast<uint64_t>(kRstBurstClearMs) * 1000ULL);
        ESP_LOGI(TAG, "press EN/RST %lu more time(s) within %lu ms for factory reset",
                 static_cast<unsigned long>(kRstBurstNeed - count),
                 static_cast<unsigned long>(kRstBurstClearMs));
    }
    return false;
}

} // namespace embed
