#pragma once

#include "embed/embed.hpp"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

namespace embed {

// ── Forward declarations for WiFi states ────────────────────────────────

struct WifiIdleState;
struct WifiScanningState;
struct WifiConnectingState;
struct WifiConnectedState;
struct WifiDisconnectedState;
struct WifiErrorState;

// ── WiFi events (state machine inputs) ──────────────────────────────────

struct WifiStartEvent {};
struct WifiScanEvent {};

struct WifiConnectEvent {
    embed::string<63> ssid;
    embed::string<63> password;
};

struct WifiGotIpEvent {
    embed::string<17> ip;
};

struct WifiDisconnectEvent {
    uint8_t reason = 0;
};

struct WifiErrorEvent {
    esp_err_t error = ESP_OK;
};

// ── Framework messages (signals — must be trivially-copyable) ────────────

/// Emitted when WiFi connects and gets an IP address.
struct WifiConnected {
    embed::string<17> ip;
};
static_assert(embed::Message<WifiConnected>);

/// Emitted when WiFi disconnects or enters error state.
struct WifiDisconnected {
    uint8_t reason = 0;
};
static_assert(embed::Message<WifiDisconnected>);

// ── WiFi states with state machine transitions ──────────────────────────

struct WifiIdleState : State<
    On<WifiStartEvent, WifiScanningState>
> {};

struct WifiScanningState : State<
    On<WifiConnectEvent, WifiConnectingState>,
    On<WifiErrorEvent, WifiErrorState>
> {};

struct WifiConnectingState : State<
    On<WifiGotIpEvent, WifiConnectedState>,
    On<WifiDisconnectEvent, WifiDisconnectedState>,
    On<WifiErrorEvent, WifiErrorState>
> {};

struct WifiConnectedState : State<
    On<WifiDisconnectEvent, WifiDisconnectedState>,
    On<WifiErrorEvent, WifiErrorState>
> {};

struct WifiDisconnectedState : State<
    On<WifiConnectEvent, WifiConnectingState>,
    On<WifiErrorEvent, WifiErrorState>
> {};

struct WifiErrorState : State<
    On<WifiStartEvent, WifiIdleState>,
    On<WifiConnectEvent, WifiConnectingState>
> {};

// ── WifiService ─────────────────────────────────────────────────────────

/// WiFi STA service with state machine and signal-slot integration.
///
/// Manages WiFi connection lifecycle using a state machine.
/// Emits framework signals (WifiConnected, WifiDisconnected)
/// that other services can subscribe to via Slot<>.
///
/// Usage:
///   auto* wifi = registry.createService<WifiService>();
///   registry.startAll();  // calls wifi->start() automatically
///   // Other services can connect to wifi->onConnected and wifi->onDisconnected
class WifiService : public Service,
                    public StateMachine<WifiService,
                                        WifiIdleState,
                                        WifiScanningState,
                                        WifiConnectingState,
                                        WifiConnectedState,
                                        WifiDisconnectedState,
                                        WifiErrorState> {
public:
    const char* serviceName() const override { return "WifiService"; }

    WifiService() = default;
    ~WifiService() override;

    /// Initialize WiFi subsystem and start connection.
    /// SSID/password: NVS (`fctry` / `nvs`) if present, else Kconfig seed.
    /// Call `enableSoftAp()` before `start()` for config-portal AP mode.
    void start() override;

    /// Stop WiFi, unregister handlers, and deinitialize.
    void stop() override;

    /// SoftAP for the config portal (open or Kconfig password). Call before start().
    void enableSoftAp();
    [[nodiscard]] bool softApEnabled() const { return softAp_; }
    [[nodiscard]] const char* apSsid() const;

    /// Framework signals — other services can subscribe to these.
    Signal<WifiConnected> onConnected;
    Signal<WifiDisconnected> onDisconnected;

    // ── State transition handlers ──────────────────────────────────────

    void onStateChanged(const TransitionTo<WifiScanningState>&) {
        ESP_LOGI("WifiSM", "[%s] -> Scanning", currentStateName());
    }

    void onStateChanged(const TransitionTo<WifiConnectingState>&) {
        ESP_LOGI("WifiSM", "[%s] -> Connecting", currentStateName());
    }

    void onStateChanged(const TransitionTo<WifiConnectedState>&) {
        retryCount_ = 0;
        ESP_LOGI("WifiSM", "[%s] -> Connected", currentStateName());
        WifiConnected msg{};
        esp_netif_ip_info_t ipInfo{};
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ipInfo) == ESP_OK) {
            snprintf(msg.ip.data(), msg.ip.capacity() + 1, IPSTR, IP2STR(&ipInfo.ip));
        }
        onConnected.emit(msg);
    }

    void onStateChanged(const TransitionTo<WifiDisconnectedState>&) {
        retryCount_++;
        ESP_LOGI("WifiSM", "[%s] -> Disconnected (retry %d/%d)",
                 currentStateName(), retryCount_, CONFIG_EMBED_WIFI_MAX_RETRY);
        if (std::holds_alternative<WifiConnectedState*>(getPrevState())) {
            onDisconnected.emit(WifiDisconnected{});
        }
    }

    void onStateChanged(const TransitionTo<WifiErrorState>&) {
        ESP_LOGI("WifiSM", "[%s] -> Error", currentStateName());
        if (std::holds_alternative<WifiConnectedState*>(getPrevState())) {
            onDisconnected.emit(WifiDisconnected{});
        }
    }

    void onStateChanged(const TransitionTo<WifiIdleState>&) {
        ESP_LOGI("WifiSM", "[%s] -> Idle", currentStateName());
    }

    void onStateChanged(const Nothing&) {
        ESP_LOGD("WifiSM", "[%s] no transition", currentStateName());
    }

    /// Get current state name for logging.
    const char* currentStateName() const {
        return std::visit([](auto* statePtr) -> const char* {
            using S = std::decay_t<decltype(*statePtr)>;
            if constexpr (std::is_same_v<S, WifiIdleState>) return "Idle";
            else if constexpr (std::is_same_v<S, WifiScanningState>) return "Scanning";
            else if constexpr (std::is_same_v<S, WifiConnectingState>) return "Connecting";
            else if constexpr (std::is_same_v<S, WifiConnectedState>) return "Connected";
            else if constexpr (std::is_same_v<S, WifiDisconnectedState>) return "Disconnected";
            else if constexpr (std::is_same_v<S, WifiErrorState>) return "Error";
            else return "Unknown";
        }, getCurrentState());
    }

private:
    int retryCount_ = 0;
    bool wifiInitialized_ = false;
    bool softAp_ = false;
    wifi_config_t wifiConfig_ = {};

    // Handler instances for the default ESP-IDF event loop
    // (esp_wifi posts to the default loop, not our custom one)
    esp_event_handler_instance_t wifiEventHandler_ = nullptr;
    esp_event_handler_instance_t ipEventHandler_ = nullptr;

    static void wifiEventHandler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data);
    static void ipEventHandler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

    esp_err_t initEventLoop();
    esp_err_t initWifiSdk();
    void processWifiEvent(int32_t event_id, void* event_data);
    void processIpEvent(int32_t event_id, void* event_data);
};

} // namespace embed
