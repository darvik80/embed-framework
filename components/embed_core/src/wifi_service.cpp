#include "embed_core/wifi_service.hpp"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"
#include <cstring>

namespace embed {

static const char* TAG = "WifiService";

WifiService::~WifiService() {
    stop();
}

void WifiService::start() {
    ESP_LOGI(TAG, "Starting WifiService");

    // NVS must be initialized before WiFi (WiFi stores calibration data in NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    ret = initEventLoop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize event loop: %s", esp_err_to_name(ret));
        return;
    }

    // Transition to scanning state before starting WiFi SDK,
    // so event handlers see the correct state
    handle(WifiStartEvent{});

    ret = initWifiSdk();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        handle(WifiErrorEvent{.error = ret});
        return;
    }

    ESP_LOGI(TAG, "WifiService started successfully");
}

void WifiService::stop() {
    if (!wifiInitialized_) return;
    ESP_LOGI(TAG, "Stopping WifiService");

    if (wifiEventHandler_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventHandler_);
        wifiEventHandler_ = nullptr;
    }
    if (ipEventHandler_) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipEventHandler_);
        ipEventHandler_ = nullptr;
    }

    esp_wifi_stop();
    esp_wifi_deinit();
    wifiInitialized_ = false;
}

void WifiService::wifiEventHandler(void* arg, esp_event_base_t /*event_base*/,
                                   int32_t event_id, void* event_data) {
    auto* service = static_cast<WifiService*>(arg);
    service->processWifiEvent(event_id, event_data);
}

void WifiService::ipEventHandler(void* arg, esp_event_base_t /*event_base*/,
                                 int32_t event_id, void* event_data) {
    auto* service = static_cast<WifiService*>(arg);
    service->processIpEvent(event_id, event_data);
}

esp_err_t WifiService::initEventLoop() {
    // Create default event loop (required by esp_wifi)
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register WiFi event handler on default event loop
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifiEventHandler, this,
                                               &wifiEventHandler_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register IP event handler on default event loop
    ret = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                               &ipEventHandler, this,
                                               &ipEventHandler_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP handler: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t WifiService::initWifiSdk() {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_config_t cfg_sta{
        .sta = {
            .ssid = CONFIG_EMBED_WIFI_SSID,
            .password = CONFIG_EMBED_WIFI_PASSWORD,
        },
    };

    cfg_sta.sta.threshold.authmode =
        std::strlen(CONFIG_EMBED_WIFI_PASSWORD) == 0
            ? WIFI_AUTH_OPEN
            : WIFI_AUTH_WPA_WPA2_PSK;

    ret = esp_wifi_set_config(WIFI_IF_STA, &cfg_sta);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    wifiInitialized_ = true;
    return ESP_OK;
}

void WifiService::processWifiEvent(int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "WiFi event: %ld, state: %s", event_id, currentStateName());

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WiFi started, connecting...");
        esp_wifi_connect();
        handle(WifiConnectEvent{});
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WiFi associated with AP");
        break;

    case WIFI_EVENT_STA_DISCONNECTED: {
        auto* event = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        ESP_LOGW(TAG, "WiFi disconnected: reason=%d", event->reason);
        handle(WifiDisconnectEvent{.reason = event->reason});

        if (retryCount_ <= CONFIG_EMBED_WIFI_MAX_RETRY) {
            ESP_LOGI(TAG, "Attempting reconnect...");
            esp_wifi_connect();
            handle(WifiConnectEvent{});
        } else {
            ESP_LOGE(TAG, "Max retries (%d) reached, giving up", CONFIG_EMBED_WIFI_MAX_RETRY);
            handle(WifiErrorEvent{.error = ESP_ERR_TIMEOUT});
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Unhandled WiFi event: %ld", event_id);
        break;
    }
}

void WifiService::processIpEvent(int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "IP event: %ld, state: %s", event_id, currentStateName());

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        WifiGotIpEvent gotIp{};
        snprintf(gotIp.ip.data(), gotIp.ip.capacity() + 1, IPSTR, IP2STR(&event->ip_info.ip));
        handle(gotIp);
        break;
    }

    case IP_EVENT_STA_LOST_IP:
        ESP_LOGI(TAG, "Lost IP");
        handle(WifiDisconnectEvent{.reason = WIFI_REASON_STA_LEAVING});
        break;

    default:
        ESP_LOGD(TAG, "Unhandled IP event: %ld", event_id);
        break;
    }
}

} // namespace embed
