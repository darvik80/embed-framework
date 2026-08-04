#include "esp_crt_bundle.h"
#include "embed/embed.hpp"
#include "embed_core/wifi_service.hpp"
#include "embed_core/metrics_service.hpp"
#include "embed_core/mqtt_credentials.hpp"
#include "embed_core/mqtt_service.hpp"
#include "crearts_iot/crearts_iot.hpp"

#include "esp_tls.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr char TAG[] = "Main";

/// Static reported attributes (app identity). version / app-name prefer
/// esp_app_desc; these label the product face of the firmware.
constexpr char kAppModel[] = "ESP32-S3";
constexpr char kAppName[] = "embed-framework";
constexpr char kAppVendor[] = "crearts";
constexpr char kAppProtocol[] = "iot/v1";

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

// ── CreartsDeviceInfo ───────────────────────────────────────────────────

/// On each MQTT connect: publish **reported** static attrs (feeds dashboard
/// reported form), request **desired**, and apply desired pushes from the
/// device-page editor (`attributes/update`).
class CreartsDeviceInfo : public embed::Service {
public:
    const char* serviceName() const override { return "CreartsDeviceInfo"; }

    void start() override {
        auto& reg = embed::ServiceRegistry::instance();
        iot_ = reg.getService<crearts::iot::CreartsIotService>();
        mqtt_ = reg.getService<embed::MqttService>();
        if (!iot_ || !mqtt_) {
            ESP_LOGE("CreartsInfo", "CreartsIotService/MqttService missing");
            return;
        }
        connectedSlot_.connect(mqtt_->onConnected);
        attrUpdateSlot_.connect(iot_->onAttributeUpdate);
        attrResponseSlot_.connect(iot_->onAttributeResponse);
        ESP_LOGI("CreartsInfo", "Will report/request attributes on MQTT connect");
    }

    void stop() override {
        connectedSlot_.disconnect();
        attrUpdateSlot_.disconnect();
        attrResponseSlot_.disconnect();
        iot_ = nullptr;
        mqtt_ = nullptr;
    }

private:
    crearts::iot::CreartsIotService* iot_ = nullptr;
    embed::MqttService* mqtt_ = nullptr;
    embed::Slot<embed::MqttConnected> connectedSlot_{onConnected, this};
    embed::Slot<crearts::iot::AttributeUpdate> attrUpdateSlot_{onAttrUpdate, this};
    embed::Slot<crearts::iot::AttributeResponse> attrResponseSlot_{onAttrResponse, this};

    static const char* chipModelName(esp_chip_model_t model) {
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

    static void publishReported(crearts::iot::CreartsIotService* iot) {
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

        // Flat object = reported scope (dashboard Properties → reported form).
        crearts::iot::AttributeBuilder attrs;
        attrs.add("version", version)
            .add("firmwareVersion", version)
            .add("model", kAppModel)
            .add("appName", appName)
            .add("vendor", kAppVendor)
            .add("protocol", kAppProtocol)
            .add("productId", CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID)
            .add("deviceId", CONFIG_EMBED_CREARTS_IOT_DEVICE_ID)
            .add("chip", chipModelName(chip.model))
            .add("chipCores", static_cast<int>(chip.cores))
            .add("idfVersion", idfVer)
            .add("mac", macStr);

        const int msgId = iot->publishAttributes(attrs, 1);
        ESP_LOGI("CreartsInfo",
                 "reported attrs msg_id=%d version=%s model=%s app=%s",
                 msgId, version, kAppModel, appName);
        iot->publishOtaVersion(version, "main", 1);
    }

    static void onConnected(const embed::MqttConnected&, void* ctx) {
        auto* self = static_cast<CreartsDeviceInfo*>(ctx);
        if (!self->iot_) return;

        publishReported(self->iot_);

        // Pull desired from platform (device-page desired form).
        crearts::iot::AttributeRequestBuilder req;
        req.desiredAll();
        uint32_t reqId = 0;
        const int msgId = self->iot_->requestAttributes(req, reqId, 1);
        ESP_LOGI("CreartsInfo", "desired request msg_id=%d id=%lu",
                 msgId, static_cast<unsigned long>(reqId));
    }

    static void onAttrUpdate(const crearts::iot::AttributeUpdate& upd, void* ctx) {
        auto* self = static_cast<CreartsDeviceInfo*>(ctx);
        // Desired push from dashboard Properties → desired form.
        auto parsed = crearts::iot::parseAttributeUpdate(
            std::string_view(upd.payload.c_str(), upd.payload.size()));
        ESP_LOGI("CreartsInfo", "desired update: %s", parsed.desiredJson.c_str());
        (void)self;
        // App-specific apply hooks go here (e.g. enable flag, intervals).
    }

    static void onAttrResponse(const crearts::iot::AttributeResponse& res, void* /*ctx*/) {
        auto parsed = crearts::iot::parseAttributeResponse(
            std::string_view(res.payload.c_str(), res.payload.size()));
        ESP_LOGI("CreartsInfo", "attr response id=%lu reported=%s desired=%s",
                 static_cast<unsigned long>(res.requestId),
                 parsed.reportedJson.c_str(),
                 parsed.desiredJson.c_str());
    }
};

// ── CreartsRpcDemo ──────────────────────────────────────────────────────

/// Handles platform RPC; built-in `reboot` (+ optional delayMs), else echoes ok.
class CreartsRpcDemo : public embed::Service {
public:
    const char* serviceName() const override { return "CreartsRpcDemo"; }

    void start() override {
        auto* iot = embed::ServiceRegistry::instance()
                        .getService<crearts::iot::CreartsIotService>();
        if (!iot) {
            ESP_LOGE("CreartsRpc", "CreartsIotService not found");
            return;
        }
        iot_ = iot;
        rpcSlot_.connect(iot->onRpcRequest);
        ESP_LOGI("CreartsRpc", "RPC demo listening (reboot supported)");
    }

    void stop() override {
        rpcSlot_.disconnect();
        iot_ = nullptr;
    }

private:
    crearts::iot::CreartsIotService* iot_ = nullptr;
    embed::Slot<crearts::iot::RpcRequest> rpcSlot_{onRpc, this};

    static int parseDelayMs(const char* params) {
        if (!params) return 0;
        const char* p = std::strstr(params, "delayMs");
        if (!p) return 0;
        p = std::strchr(p, ':');
        if (!p) return 0;
        return std::atoi(p + 1);
    }

    static void rebootTask(void* arg) {
        const auto delayMs = reinterpret_cast<uintptr_t>(arg);
        if (delayMs > 0) {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
        ESP_LOGW("CreartsRpc", "Rebooting now");
        esp_restart();
    }

    static void onRpc(const crearts::iot::RpcRequest& req, void* ctx) {
        auto* self = static_cast<CreartsRpcDemo*>(ctx);
        ESP_LOGI("CreartsRpc", "RPC id=%lu method=%s params=%s",
                 static_cast<unsigned long>(req.requestId),
                 req.method.c_str(),
                 req.params.c_str());
        if (!self->iot_) return;

        if (req.method == "reboot") {
            int delayMs = parseDelayMs(req.params.c_str());
            if (delayMs < 0) delayMs = 0;
            if (delayMs > 60000) delayMs = 60000;
            self->iot_->respondRpc(req.requestId, 0, "ok",
                                   "{\"rebooting\":true}");
            // Respond first, then reboot from a task so MQTT can flush.
            xTaskCreate(rebootTask, "rpc_reboot", 2048,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(delayMs > 0 ? delayMs : 500)),
                        5, nullptr);
            return;
        }

        self->iot_->respondRpc(req.requestId, 0, "ok",
                               req.params.empty() ? "{}" : req.params.c_str());
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
        ESP_LOGI("Monitor", "METRICS: cpu=%u%% heap_free=%u uptime=%us wifi=%s",
                 msg.cpuUsagePercent, msg.freeHeap, msg.uptimeSeconds,
                 msg.wifiConnected ? "UP" : "DOWN");
    }

    static void onMqttConnected(const embed::MqttConnected& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "MQTT CONNECTED to %s", msg.brokerUri.c_str());
    }

    static void onMqttDisconnected(const embed::MqttDisconnected& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "MQTT DISCONNECTED, reason=%u", msg.reason);
    }

    static void onMqttMessage(const embed::MqttMessageReceived& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "MQTT MSG: topic=%s len=%u", msg.topic.c_str(), msg.payload.size());
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

    esp_err_t ret = esp_tls_init_global_ca_store();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init global CA store: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Loading CRT bundle...");
    ret = esp_crt_bundle_attach(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set CA bundle: %s", esp_err_to_name(ret));
        return;
    }

    embed::EventLoop::instance().init();

    if (CONFIG_EMBED_CREARTS_IOT_HOST[0] == '\0' ||
        CONFIG_EMBED_CREARTS_IOT_ACCESS_TOKEN[0] == '\0' ||
        CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID[0] == '\0' ||
        CONFIG_EMBED_CREARTS_IOT_DEVICE_ID[0] == '\0') {
        ESP_LOGE(TAG,
                 "Crearts Kconfig incomplete — set EMBED_CREARTS_IOT_* "
                 "(product/device/host/token) in menuconfig");
        return;
    }

    const auto topicStyle =
#ifdef CONFIG_EMBED_CREARTS_IOT_TOPIC_SHORT
        crearts::iot::TopicStyle::Short;
#else
        crearts::iot::TopicStyle::Full;
#endif

    static auto creartsCreds = crearts::iot::CreartsCredentials::createAccessToken(
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

    if (!creartsCreds || !creartsCreds->isValid()) {
        ESP_LOGE(TAG, "Crearts credentials invalid — abort");
        return;
    }

    ESP_LOGI(TAG, "Crearts MQTT client_id=%s uri=%s style=%s",
             creartsCreds->clientId(), creartsCreds->brokerUri(),
#ifdef CONFIG_EMBED_CREARTS_IOT_TOPIC_SHORT
             "short"
#else
             "full"
#endif
    );

    auto& registry = embed::ServiceRegistry::instance();
    registry.createService<embed::WifiService>();
    registry.createService<embed::MetricsService>();
    registry.createService<embed::MqttService>(*creartsCreds);
    registry.createService<crearts::iot::CreartsIotService>(*creartsCreds);
    registry.createService<crearts::iot::MetricsTelemetryBridge>();
    registry.createService<CreartsDeviceInfo>();
    registry.createService<CreartsRpcDemo>();
    registry.createService<MonitorService>();

    ESP_LOGI(TAG, "services=%zu free_heap=%lu",
             registry.count(),
             static_cast<unsigned long>(esp_get_free_heap_size()));

    registry.startAll();
    ESP_LOGI(TAG, "running — WiFi+MQTT → Crearts (%s.%s)",
             CONFIG_EMBED_CREARTS_IOT_PRODUCT_ID,
             CONFIG_EMBED_CREARTS_IOT_DEVICE_ID);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10'000));
    }
}
