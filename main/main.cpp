#include "esp_crt_bundle.h"
#include "embed/embed.hpp"
#include "embed_core/wifi_service.hpp"
#include "embed_core/metrics_service.hpp"
#include "embed_core/mqtt_credentials.hpp"
#include "thingsboard/credentials.h"
#include "embed_core/mqtt_service.hpp"
#include "embed_extra/camera_service.hpp"
#include "embed_extra/mjpeg_service.hpp"
#include "alicloud_oss/oss_service.hpp"
#include "alicloud_iot/alicloud_credentials.hpp"
#include "alicloud_iot/alicloud_service.hpp"

#include "esp_tls.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "embed_extra/oss_upload_service.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

// ── PlainMqttCredentials ────────────────────────────────────────────────

/// Simple MqttCredentials implementation for plain MQTT brokers.
/// Stores URI, client ID, username, and password in fixed-size strings.
class PlainMqttCredentials : public embed::MqttCredentials {
public:
    PlainMqttCredentials(const char* uri, const char* clientId,
                         const char* user, const char* pass)
        : uri_(uri), clientId_(clientId), username_(user), password_(pass) {}

    const char* brokerUri() const override { return uri_.c_str(); }
    const char* clientId() const override { return clientId_.c_str(); }
    const char* username() const override { return username_.c_str(); }
    const char* password() const override { return password_.c_str(); }

private:
    embed::string<127> uri_;
    embed::string<63> clientId_;
    embed::string<63> username_;
    embed::string<63> password_;
};

// ── MonitorService ──────────────────────────────────────────────────────

class MonitorService : public embed::Service {
public:
    const char* serviceName() const override { return "MonitorService"; }

    void start() override {
        auto& reg = embed::ServiceRegistry::instance();
        auto* blink = reg.getService<BlinkService>();
        auto* button = reg.getService<ButtonSimService>();
        auto* wifi = reg.getService<embed::WifiService>();
        auto* metrics = reg.getService<embed::MetricsService>();
        auto* mqtt = reg.getService<embed::MqttService>();

        if (blink) led_slot_.connect(blink->onLedChanged);
        if (button) button_slot_.connect(button->onButtonPressed);
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
        ESP_LOGI("Monitor", "slots connected: blink=%d button=%d wifi=%d metrics=%d mqtt=%d",
                 blink != nullptr, button != nullptr, wifi != nullptr,
                 metrics != nullptr, mqtt != nullptr);
    }

private:
    static void onLedChanged(const LedStateChanged& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "LED GPIO %d -> %s", msg.gpio, msg.on ? "ON" : "OFF");
    }

    static void onButton(const ButtonPressed& msg, void* /*ctx*/) {
        ESP_LOGI("Monitor", "Button GPIO %d, level=%d", msg.gpio, msg.level);
    }

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

    embed::Slot<LedStateChanged> led_slot_{onLedChanged, this};
    embed::Slot<ButtonPressed> button_slot_{onButton, this};
    embed::Slot<embed::WifiConnected> wifi_connected_slot_{onWifiConnected, this};
    embed::Slot<embed::WifiDisconnected> wifi_disconnected_slot_{onWifiDisconnected, this};
    embed::Slot<embed::MetricsCollected> metrics_slot_{onMetricsCollected, this};
    embed::Slot<embed::MqttConnected> mqtt_connected_slot_{onMqttConnected, this};
    embed::Slot<embed::MqttDisconnected> mqtt_disconnected_slot_{onMqttDisconnected, this};
    embed::Slot<embed::MqttMessageReceived> mqtt_message_slot_{onMqttMessage, this};
};

// ── app_main ────────────────────────────────────────────────────────────

extern "C" void app_main() {
    ESP_LOGI("Main", "embed-framework demo starting...");

    esp_err_t ret = esp_tls_init_global_ca_store();
    if (ret != ESP_OK)
    {
        ESP_LOGE("app", "Failed to init global CA store: %s", esp_err_to_name(ret));
        return;
    }

    // // Feed RTC WDT before heavy CRT bundle loading to prevent watchdog reset
    // esp_task_wdt_reset();
    ESP_LOGI("Main", "Loading CRT bundle (may take a few seconds)...");
    ret = esp_crt_bundle_attach(NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE("app", "Failed to set CA bundle: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("Main", "CRT bundle loaded successfully");

    // 1. Initialize the event loop
    embed::EventLoop::instance().init();
    ESP_LOGI("Main", "event loop initialized");

    // 2. Create MQTT credentials (static — must outlive MqttService)
    //    Use AlicloudCredentials if configured; fall back to PlainMqttCredentials.
    static auto aliCreds = alicloud::iot::AlicloudCredentials::create(
        CONFIG_EMBED_ALICLOUD_PRODUCT_KEY,
        CONFIG_EMBED_ALICLOUD_DEVICE_NAME,
        CONFIG_EMBED_ALICLOUD_DEVICE_SECRET
    );

    static PlainMqttCredentials plainCreds(
        CONFIG_EMBED_MQTT_BROKER_URI,
        CONFIG_EMBED_MQTT_CLIENT_ID,
        CONFIG_EMBED_MQTT_USERNAME,
        CONFIG_EMBED_MQTT_PASSWORD
    );

    embed::MqttCredentials& mqttCreds = (aliCreds && aliCreds->isValid())
        ? static_cast<embed::MqttCredentials&>(*aliCreds)
        : static_cast<embed::MqttCredentials&>(plainCreds);

    if (aliCreds && aliCreds->isValid())
        ESP_LOGI("Main", "Using Alibaba Cloud IoT credentials");
    else
        ESP_LOGW("Main", "Using plain MQTT credentials (Alicloud not configured)");

    // 3. Create services
    ESP_LOGI("Main", "Free heap before services: %lu", esp_get_free_heap_size());
    auto& registry = embed::ServiceRegistry::instance();
    //registry.createService<BlinkService>(GPIO_NUM_2);
    //registry.createService<ButtonSimService>(GPIO_NUM_0);
    registry.createService<embed::WifiService>();
    //registry.createService<embed::MetricsService>();
    //registry.createService<embed::MqttService>(mqttCreds);
    //registry.createService<alicloud::iot::AlicloudService>();
    registry.createService<embed::CameraService>();
    registry.createService<embed::MjpegService>();
    //registry.createService<embed::OssService>();
    // OssUploadService disabled — MjpegService is the sole frame consumer
    //registry.createService<embed::OssUploadService>();
    //registry.createService<MonitorService>();

    ESP_LOGI("Main", "services created: %zu, free heap: %lu",
             registry.count(), esp_get_free_heap_size());

    // 4. Verify getService with dynamic_cast
    auto* found_blink = registry.getService<BlinkService>();
    auto* found_wifi = registry.getService<embed::WifiService>();

    ESP_LOGI("Main", "getService<BlinkService> -> %s",
             found_blink ? "found" : "NOT FOUND");
    ESP_LOGI("Main", "getService<WifiService> -> %s",
             found_wifi ? "found" : "NOT FOUND");

    // Verify dynamic_cast correctly distinguishes types
    auto* wrong_cast = dynamic_cast<embed::WifiService*>(
        static_cast<embed::Service*>(found_blink));
    ESP_LOGI("Main", "dynamic_cast<BlinkService->WifiService> -> %s (should be nullptr)",
             wrong_cast ? "NON-NULL (ERROR!)" : "nullptr (correct!)");

    // 5. Start all services (lifecycle managed by registry)
    registry.startAll();

    ESP_LOGI("Main", "demo running — LED blinks every 500ms, metrics every 10s, WiFi+MQTT connecting...");

    // Main task just idles — event loop runs in its own task
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10'000));
    }
}
