#include "esp_crt_bundle.h"
#include "embed/embed.hpp"
#include "embed_core/wifi_service.hpp"
#include "embed_core/metrics_service.hpp"
#include "embed_core/mqtt_credentials.hpp"
#include "embed_core/mqtt_service.hpp"
#include "embed_core/nvs_store.hpp"
#include "embed_core/device_settings.hpp"
#include "embed_core/firmware_slot.hpp"
#include "embed_core/config_portal_service.hpp"
#include "crearts_iot/crearts_iot.hpp"
#include "embed_extra/led_strip_service.hpp"

#include "esp_tls.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "cJSON.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

namespace {

constexpr char TAG[] = "Main";

} // namespace

// ── Messages ────────────────────────────────────────────────────────────

struct LedStateChanged {
    int gpio;
    bool on;
};
static_assert(embed::Message<LedStateChanged>);

struct ButtonPressed {
    int gpio;
    bool level;
};
static_assert(embed::Message<ButtonPressed>);

// ── BlinkService ────────────────────────────────────────────────────────

class BlinkService : public embed::Service {
public:
    const char* serviceName() const override { return "BlinkService"; }

    embed::Signal<LedStateChanged> onLedChanged;

    explicit BlinkService(int gpio = GPIO_NUM_2)
        : led_gpio_(gpio), led_on_(false)
    {
        gpio_reset_pin(static_cast<gpio_num_t>(led_gpio_));
        gpio_set_direction(static_cast<gpio_num_t>(led_gpio_), GPIO_MODE_OUTPUT);
    }

    void start() override {
        const esp_timer_create_args_t args = {
            .callback = timerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "blink_timer",
            .skip_unhandled_events = true
        };
        esp_timer_create(&args, &timer_);
        esp_timer_start_periodic(timer_, 500'000); // 500ms
    }

    void stop() override {
        if (timer_) {
            esp_timer_stop(timer_);
            esp_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

    void toggle() {
        led_on_ = !led_on_;
        gpio_set_level(static_cast<gpio_num_t>(led_gpio_), led_on_ ? 1 : 0);
        onLedChanged.emit({led_gpio_, led_on_});
    }

private:
    static void timerCallback(void* arg) {
        auto* self = static_cast<BlinkService*>(arg);
        self->toggle();
    }

    int led_gpio_;
    bool led_on_;
    esp_timer_handle_t timer_ = nullptr;
};

// ── ButtonSimService ────────────────────────────────────────────────────

class ButtonSimService : public embed::Service {
public:
    const char* serviceName() const override { return "ButtonSimService"; }

    embed::Signal<ButtonPressed> onButtonPressed;

    explicit ButtonSimService(int gpio = GPIO_NUM_0)
        : button_gpio_(gpio), press_count_(0)
    {}

    void start() override {
        const esp_timer_create_args_t args = {
            .callback = timerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "button_sim_timer",
            .skip_unhandled_events = true
        };
        esp_timer_create(&args, &timer_);
        esp_timer_start_periodic(timer_, 2'000'000); // 2s
    }

    void stop() override {
        if (timer_) {
            esp_timer_stop(timer_);
            esp_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

private:
    static void timerCallback(void* arg) {
        auto* self = static_cast<ButtonSimService*>(arg);
        self->press_count_++;
        ESP_LOGI("ButtonSim", "simulating press #%u on GPIO %d",
                 static_cast<unsigned>(self->press_count_), self->button_gpio_);
        self->onButtonPressed.emit({self->button_gpio_, false});
    }

    int button_gpio_;
    uint32_t press_count_;
    esp_timer_handle_t timer_ = nullptr;
};

// ── CreartsRpcDemo ──────────────────────────────────────────────────────

/// Registers firmware RPC methods on CreartsIotService::rpc() (`rpc-list` is built-in).
class CreartsRpcDemo : public embed::Service {
public:
    const char* serviceName() const override { return "CreartsRpcDemo"; }

    void start() override {
        auto* iot = embed::ServiceRegistry::instance()
                        .getService<crearts::iot::IotService>();
        if (!iot) {
            ESP_LOGE("CreartsRpc", "CreartsIotService not found");
            return;
        }

        using crearts::iot::rpcBool;
        using crearts::iot::rpcInt;
        using crearts::iot::rpcStr;
        auto& rpc = iot->rpc();

        static constexpr crearts::iot::RpcParamDef kEcho[] = {rpcStr("msg")};
        static constexpr crearts::iot::RpcParamDef kLedAttach[] = {
            rpcInt("gpio"), rpcInt("count"), rpcInt("brightness", false, 0)};
        static constexpr crearts::iot::RpcParamDef kLedDetach[] = {rpcInt("gpio")};
        static constexpr crearts::iot::RpcParamDef kSetLed[] = {
            rpcInt("gpio"),
            rpcInt("offset"),
            rpcInt("length"),
            rpcInt("r", false, 255),
            rpcInt("g", false, 255),
            rpcInt("b", false, 255),
            rpcBool("on", false, true)};
        static constexpr crearts::iot::RpcParamDef kReboot[] = {rpcInt("delayMs", false, 500)};
        static constexpr crearts::iot::RpcParamDef kFactoryReset[] = {rpcBool("confirm")};
        static constexpr crearts::iot::RpcParamDef kImportCreds[] = {rpcStr("json")};
        static constexpr crearts::iot::RpcParamDef kExportCreds[] = {rpcBool("secrets", false, false)};

        rpc.add("echo", kEcho, onEcho, nullptr, "Echo a string");
        rpc.add("led_attach", kLedAttach, onLedAttach, nullptr, "Attach WS2812 strip on GPIO");
        rpc.add("led_detach", kLedDetach, onLedDetach, nullptr, "Detach strip on GPIO");
        rpc.add("led_list", onLedList, nullptr, "List attached strips");
        rpc.add("set_led", kSetLed, onSetLed, nullptr, "Set LED range color");
        rpc.add("reboot", kReboot, onReboot, nullptr, "Reboot device");
        rpc.add("restart", kReboot, onReboot, nullptr, "Restart device");
        rpc.add("factory_reset", kFactoryReset, onFactoryReset, nullptr,
                "Wipe fctry WiFi+token and reboot into config AP");
        rpc.add("config_portal", onConfigPortal, nullptr,
                "Reboot into config AP without wiping credentials");
        rpc.add("ota_rollback", kFactoryReset, onOtaRollback, nullptr,
                "Boot previous OTA slot (confirm=true)");
        rpc.add("import_credentials", kImportCreds, onImportCredentials, nullptr,
                "Import WiFi+Crearts from JSON string (params.json)");
        rpc.add("export_credentials", kExportCreds, onExportCredentials, nullptr,
                "Export credentials JSON (secrets=true includes token)");

        ESP_LOGI("CreartsRpc", "RPC registered (%u + rpc-list)", rpc.count());
    }

    void stop() override {}

private:
    static void replyJson(crearts::iot::IotService& iot,
                          uint32_t id,
                          int code,
                          const char* message,
                          cJSON* data)
    {
        char* printed = data ? cJSON_PrintUnformatted(data) : nullptr;
        iot.respondRpc(id, code, message, printed ? printed : "{}");
        if (printed) free(printed);
        if (data) cJSON_Delete(data);
    }

    static void rebootTask(void* arg) {
        const auto delayMs = reinterpret_cast<uintptr_t>(arg);
        if (delayMs > 0) {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
        ESP_LOGW("CreartsRpc", "Rebooting now");
        esp_restart();
    }

    static embed::LedStripService* leds() {
        return embed::ServiceRegistry::instance().getService<embed::LedStripService>();
    }

    static int clampByte(int v) {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return v;
    }

    static void onEcho(crearts::iot::IotService& iot,
                       uint32_t id,
                       const crearts::iot::RpcParams& p,
                       void*)
    {
        std::string msg;
        if (!p.get("msg", msg)) {
            iot.respondRpc(id, 400, "missing params.msg");
            return;
        }
        ESP_LOGW("CreartsRpc", "Echo: %s", msg.c_str());
        cJSON* data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "msg", msg.c_str());
        replyJson(iot, id, 0, "ok", data);
    }

    static void onLedAttach(crearts::iot::IotService& iot,
                            uint32_t id,
                            const crearts::iot::RpcParams& p,
                            void*)
    {
        auto* strip = leds();
        if (!strip) {
            iot.respondRpc(id, 503, "led strip service not available");
            return;
        }
        int gpio = 0;
        int count = 0;
        if (!p.get("gpio", gpio) || !p.get("count", count)) {
            iot.respondRpc(id, 400, "params.gpio and params.count required");
            return;
        }
        if (gpio < 0 || count < 1 || count > 256) {
            iot.respondRpc(id, 400, "invalid gpio/count");
            return;
        }
        const int bri = clampByte(p.getInt("brightness", 0));
        if (!strip->attach(gpio, static_cast<uint16_t>(count), static_cast<uint8_t>(bri))) {
            iot.respondRpc(id, 500, "led_attach failed (gpio busy / no RMT slot)");
            return;
        }
        cJSON* data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "gpio", gpio);
        cJSON_AddNumberToObject(data, "count", count);
        cJSON_AddNumberToObject(data, "brightness", strip->brightness(gpio));
        replyJson(iot, id, 0, "ok", data);
    }

    static void onLedDetach(crearts::iot::IotService& iot,
                            uint32_t id,
                            const crearts::iot::RpcParams& p,
                            void*)
    {
        auto* strip = leds();
        if (!strip) {
            iot.respondRpc(id, 503, "led strip service not available");
            return;
        }
        int gpio = 0;
        if (!p.get("gpio", gpio)) {
            iot.respondRpc(id, 400, "params.gpio required");
            return;
        }
        if (!strip->detach(gpio)) {
            iot.respondRpc(id, 404, "strip not attached on this gpio");
            return;
        }
        cJSON* data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "gpio", gpio);
        replyJson(iot, id, 0, "ok", data);
    }

    static void onLedList(crearts::iot::IotService& iot,
                          uint32_t id,
                          const crearts::iot::RpcParams&,
                          void*)
    {
        auto* strip = leds();
        if (!strip) {
            iot.respondRpc(id, 503, "led strip service not available");
            return;
        }
        embed::LedStripInfo infos[8] = {};
        const uint8_t n = strip->list(infos, 8);
        cJSON* data = cJSON_CreateObject();
        cJSON* arr = cJSON_AddArrayToObject(data, "strips");
        for (uint8_t i = 0; i < n; ++i) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "gpio", infos[i].gpio);
            cJSON_AddNumberToObject(item, "count", infos[i].count);
            cJSON_AddNumberToObject(item, "brightness", infos[i].brightness);
            cJSON_AddItemToArray(arr, item);
        }
        replyJson(iot, id, 0, "ok", data);
    }

    static void onSetLed(crearts::iot::IotService& iot,
                         uint32_t id,
                         const crearts::iot::RpcParams& p,
                         void*)
    {
        auto* strip = leds();
        if (!strip) {
            iot.respondRpc(id, 503, "led strip service not available");
            return;
        }

        int gpio = 0;
        int offset = 0;
        int length = 0;
        if (!p.get("gpio", gpio) || !p.get("offset", offset) || !p.get("length", length)) {
            iot.respondRpc(id, 400, "params.gpio, offset and length required");
            return;
        }
        if (!strip->attached(gpio)) {
            iot.respondRpc(id, 404, "strip not attached — call led_attach first");
            return;
        }
        if (offset < 0 || length < 1 ||
            static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) > strip->ledCount(gpio)) {
            iot.respondRpc(id, 400, "led range out of bounds");
            return;
        }

        const bool on = p.getBool("on", true);
        int r = clampByte(p.getInt("r", 255));
        int g = clampByte(p.getInt("g", 255));
        int b = clampByte(p.getInt("b", 255));
        if (!on) {
            r = g = b = 0;
        }

        ESP_LOGD("CreartsRpc", "set_led gpio=%d offset=%d length=%d rgb=%d,%d,%d",
                 gpio, offset, length, r, g, b);

        if (!strip->setRange(gpio, static_cast<uint16_t>(offset), static_cast<uint16_t>(length),
                             static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                             static_cast<uint8_t>(b))) {
            iot.respondRpc(id, 500, "led update failed");
            return;
        }

        cJSON* data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "gpio", gpio);
        cJSON_AddNumberToObject(data, "offset", offset);
        cJSON_AddNumberToObject(data, "length", length);
        cJSON_AddNumberToObject(data, "r", r);
        cJSON_AddNumberToObject(data, "g", g);
        cJSON_AddNumberToObject(data, "b", b);
        cJSON_AddBoolToObject(data, "on", on);
        replyJson(iot, id, 0, "ok", data);
    }

    static void onReboot(crearts::iot::IotService& iot,
                         uint32_t id,
                         const crearts::iot::RpcParams& p,
                         void*)
    {
        int delayMs = p.getInt("delayMs", 500);
        if (delayMs < 0) delayMs = 0;
        if (delayMs > 60000) delayMs = 60000;

        iot.respondRpc(id, 0, "ok", "{\"rebooting\":true}");
        xTaskCreate(rebootTask, "rpc_reboot", 2048,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(delayMs)),
                    5, nullptr);
    }

    static void onFactoryReset(crearts::iot::IotService& iot,
                               uint32_t id,
                               const crearts::iot::RpcParams& p,
                               void*)
    {
        bool confirm = false;
        if (!p.get("confirm", confirm) || !confirm) {
            iot.respondRpc(id, 400, "params.confirm=true required");
            return;
        }
        const esp_err_t err = embed::factoryResetSettings();
        if (err != ESP_OK) {
            iot.respondRpc(id, 500, esp_err_to_name(err));
            return;
        }
        iot.respondRpc(id, 0, "ok", "{\"portal\":true}");
        embed::scheduleReboot(800);
    }

    static void onConfigPortal(crearts::iot::IotService& iot,
                               uint32_t id,
                               const crearts::iot::RpcParams&,
                               void*)
    {
        const esp_err_t err = embed::requestConfigPortal();
        if (err != ESP_OK) {
            iot.respondRpc(id, 500, esp_err_to_name(err));
            return;
        }
        iot.respondRpc(id, 0, "ok", "{\"portal\":true}");
        embed::scheduleReboot(800);
    }

    static void onImportCredentials(crearts::iot::IotService& iot,
                                    uint32_t id,
                                    const crearts::iot::RpcParams& p,
                                    void*)
    {
        std::string json;
        if (!p.get("json", json) || json.empty()) {
            iot.respondRpc(id, 400, "params.json required (credentials object as string)");
            return;
        }
        const esp_err_t err = embed::importCredentialsJson(json.c_str());
        if (err == ESP_ERR_INVALID_ARG) {
            iot.respondRpc(id, 400, "invalid credentials JSON");
            return;
        }
        if (err != ESP_OK) {
            iot.respondRpc(id, 500, esp_err_to_name(err));
            return;
        }
        iot.respondRpc(id, 0, "ok", "{\"imported\":true}");
        embed::scheduleReboot(800);
    }

    static void onExportCredentials(crearts::iot::IotService& iot,
                                    uint32_t id,
                                    const crearts::iot::RpcParams& p,
                                    void*)
    {
        const bool secrets = p.getBool("secrets", false);
        char buf[2048]{};
        const esp_err_t err = embed::exportCredentialsJson(buf, sizeof(buf), secrets);
        if (err != ESP_OK) {
            iot.respondRpc(id, 500, esp_err_to_name(err));
            return;
        }
        iot.respondRpc(id, 0, "ok", buf);
    }

    static void onOtaRollback(crearts::iot::IotService& iot,
                              uint32_t id,
                              const crearts::iot::RpcParams& p,
                              void*)
    {
        bool confirm = false;
        if (!p.get("confirm", confirm) || !confirm) {
            iot.respondRpc(id, 400, "params.confirm=true required");
            return;
        }
        const esp_err_t err = embed::rollbackFirmware();
        if (err == ESP_ERR_NOT_FOUND) {
            iot.respondRpc(id, 404, "no previous firmware slot");
            return;
        }
        if (err != ESP_OK) {
            iot.respondRpc(id, 500, esp_err_to_name(err));
            return;
        }
        iot.respondRpc(id, 0, "ok", "{\"rollback\":true}");
        embed::scheduleReboot(800);
    }
};

// ── MonitorService ──────────────────────────────────────────────────────

class MonitorService : public embed::Service {
public:
    const char* serviceName() const override { return "MonitorService"; }

    void start() override {
        auto& reg = embed::ServiceRegistry::instance();
        auto* wifi = reg.getService<embed::WifiService>();
        auto* metrics = reg.getService<embed::MetricsService>();
        auto* mqtt = reg.getService<embed::MqttService>();

        if (wifi) {
            wifi_connected_slot_.connect(wifi->onConnected);
            wifi_disconnected_slot_.connect(wifi->onDisconnected);
        }
        if (metrics) metrics_slot_.connect(metrics->onMetricsCollected);
        if (mqtt) {
            mqtt_connected_slot_.connect(mqtt->onConnected);
            mqtt_disconnected_slot_.connect(mqtt->onDisconnected);
            mqtt_message_slot_.connect(mqtt->onMessage);
        }
        ESP_LOGI("Monitor", "slots connected: wifi=%d metrics=%d mqtt=%d",
                 wifi != nullptr, metrics != nullptr, mqtt != nullptr);
    }

private:
    static void onWifiConnected(const embed::WifiConnected& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "WiFi CONNECTED, IP: %s", msg.ip.c_str());
    }

    static void onWifiDisconnected(const embed::WifiDisconnected& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "WiFi DISCONNECTED, reason=%u", msg.reason);
    }

    static void onMetricsCollected(const embed::MetricsCollected& msg, void* /*ctx*/) {
        ESP_LOGD("Monitor",
                 "METRICS: cpu=%u%% heap=%u dram=%u/%u psram=%u uptime=%us wifi=%s",
                 msg.cpuUsagePercent, msg.freeHeap, msg.freeDram, msg.totalDram,
                 msg.freePsram, msg.uptimeSeconds,
                 msg.wifiConnected ? "UP" : "DOWN");
    }

    static void onMqttConnected(const embed::MqttConnected& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "MQTT CONNECTED to %s", msg.brokerUri.c_str());
    }

    static void onMqttDisconnected(const embed::MqttDisconnected& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "MQTT DISCONNECTED, reason=%u", msg.reason);
    }

    static void onMqttMessage(const embed::MqttMessageReceived& msg, void* /*ctx*/) {
        ESP_LOGD("Monitor", "MQTT MSG: topic=%s len=%u", msg.topic.c_str(), msg.payload.size());
    }

    embed::Slot<embed::WifiConnected> wifi_connected_slot_{onWifiConnected, this};
    embed::Slot<embed::WifiDisconnected> wifi_disconnected_slot_{onWifiDisconnected, this};
    embed::Slot<embed::MetricsCollected> metrics_slot_{onMetricsCollected, this};
    embed::Slot<embed::MqttConnected> mqtt_connected_slot_{onMqttConnected, this};
    embed::Slot<embed::MqttDisconnected> mqtt_disconnected_slot_{onMqttDisconnected, this};
    embed::Slot<embed::MqttMessageReceived> mqtt_message_slot_{onMqttMessage, this};
};

// ── app_main ────────────────────────────────────────────────────────────

extern "C" void app_main() {
    ESP_LOGI(TAG, "embed-framework → Crearts IoT Platform");

    // GPIO 0 is a strapping pin: held at chip RESET → UART download mode.
    // Watch BOOT while the app runs; also accept a hold already in progress.
    embed::startFactoryResetGpioWatch();

    if (embed::NvsStore::initFlash() != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed");
        embed::failUnconfirmedFirmware("nvs init failed");
        return;
    }

    const bool rstBurst = embed::checkRstBurstFactoryReset();
    const bool gpioHold = embed::factoryResetGpioHeld();
    if (rstBurst || gpioHold) {
        ESP_LOGW(TAG, "factory reset — wiping fctry + config portal (rst=%d gpio=%d)",
                 rstBurst ? 1 : 0, gpioHold ? 1 : 0);
        embed::factoryResetSettings();
    }

    // Before TLS / services — crash-loop here still increments and can roll back.
    embed::checkCrashLoopRollback();

    esp_err_t ret = esp_tls_init_global_ca_store();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init global CA store: %s", esp_err_to_name(ret));
        embed::failUnconfirmedFirmware("tls ca store failed");
        return;
    }

    ESP_LOGI(TAG, "Loading CRT bundle...");
    ret = esp_crt_bundle_attach(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set CA bundle: %s", esp_err_to_name(ret));
        embed::failUnconfirmedFirmware("crt bundle failed");
        return;
    }

    embed::EventLoop::instance().init();

    const bool portal = embed::needsConfigPortal();

    const auto topicStyle =
#ifdef CONFIG_EMBED_CREARTS_IOT_TOPIC_SHORT
        crearts::iot::TopicStyle::Short;
#else
        crearts::iot::TopicStyle::Full;
#endif

    static std::optional<crearts::iot::CreartsCredentials> creartsCreds;
    if (!portal) {
        creartsCreds = crearts::iot::loadOrSeedCredentials(
            CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID,
            CONFIG_EMBED_CREARTS_IOT_DEVICE_ID,
            CONFIG_EMBED_CREARTS_IOT_HOST,
            CONFIG_EMBED_CREARTS_IOT_ACCESS_TOKEN,
            topicStyle,
#ifdef CONFIG_EMBED_CREARTS_IOT_USE_TLS
            true,
#else
            false,
#endif
            static_cast<uint16_t>(CONFIG_EMBED_CREARTS_IOT_PORT));
    }

    auto& registry = embed::ServiceRegistry::instance();
    auto* wifi = registry.createService<embed::WifiService>();
    if (portal) {
        wifi->enableSoftAp();
    }
    registry.createService<embed::MetricsService>();
    registry.createService<embed::ConfigPortalService>();

    const bool mqttOk = !portal && creartsCreds && creartsCreds->isValid();
    if (mqttOk) {
        ESP_LOGI(TAG, "Crearts MQTT client_id=%s uri=%s style=%s",
                 creartsCreds->clientId(), creartsCreds->brokerUri(),
                 creartsCreds->topicStyle() == crearts::iot::TopicStyle::Short
                     ? "short"
                     : "full");
        registry.createService<embed::MqttService>(*creartsCreds);
        registry.createService<crearts::iot::IotService>(*creartsCreds);
        registry.createService<crearts::iot::MetricsTelemetryBridge>();
        registry.createService<crearts::iot::OtaService>();
        registry.createService<crearts::iot::NtpService>();
        registry.createService<crearts::iot::LogService>();
        registry.createService<crearts::iot::DeviceInfo>();
        registry.createService<CreartsRpcDemo>();
    } else if (!portal) {
        ESP_LOGW(TAG,
                 "Crearts creds missing — config HTTP on STA; "
                 "or hold BOOT to open SoftAP portal");
    }

#ifdef CONFIG_EMBED_LED_STRIP
    registry.createService<embed::LedStripService>();
#endif
    registry.createService<MonitorService>();

    ESP_LOGI(TAG, "services=%zu free_heap=%lu portal=%d",
             registry.count(),
             static_cast<unsigned long>(esp_get_free_heap_size()),
             portal ? 1 : 0);

    registry.startAll();
    if (portal) {
        ESP_LOGW(TAG, "CONFIG PORTAL AP=%s → http://192.168.4.1/", wifi->apSsid());
    } else if (mqttOk) {
        ESP_LOGI(TAG, "running — WiFi+MQTT → Crearts (%.*s.%.*s)",
                 static_cast<int>(creartsCreds->productId().size()),
                 creartsCreds->productId().data(),
                 static_cast<int>(creartsCreds->deviceId().size()),
                 creartsCreds->deviceId().data());
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10'000));
    }
}
