#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_credentials.hpp"
#include "embed_core/wifi_service.hpp"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "esp_log.h"

namespace embed {

// ── Forward declarations for MQTT states ─────────────────────────────────

struct MqttIdleState;
struct MqttConnectingState;
struct MqttConnectedState;
struct MqttDisconnectedState;
struct MqttErrorState;

// ── MQTT internal events (state machine inputs) ─────────────────────────

struct MqttStartEvent {};
struct MqttStopEvent {};
struct MqttBrokerConnectedEvent {};
struct MqttBrokerDisconnectedEvent { uint8_t reason = 0; };
struct MqttErrorEvent { esp_err_t error = ESP_OK; };

// ── Framework messages (signals — must be trivially-copyable) ────────────

/// Emitted when MQTT broker connection is established.
struct MqttConnected {
    embed::string<127> brokerUri;
};
static_assert(embed::Message<MqttConnected>);

/// Emitted when MQTT connection is lost.
struct MqttDisconnected {
    uint8_t reason = 0;
};
static_assert(embed::Message<MqttDisconnected>);

/// Emitted when an incoming MQTT message arrives.
struct MqttMessageReceived {
    embed::string<127> topic;
    embed::string<255> payload;
};
static_assert(embed::Message<MqttMessageReceived>);

// ── MQTT states with state machine transitions ──────────────────────────

struct MqttIdleState : State<
    On<MqttStartEvent, MqttConnectingState>
> {};

struct MqttConnectingState : State<
    On<MqttBrokerConnectedEvent, MqttConnectedState>,
    On<MqttBrokerDisconnectedEvent, MqttDisconnectedState>,
    On<MqttErrorEvent, MqttErrorState>,
    On<MqttStopEvent, MqttIdleState>
> {};

struct MqttConnectedState : State<
    On<MqttBrokerDisconnectedEvent, MqttDisconnectedState>,
    On<MqttStopEvent, MqttIdleState>
> {};

struct MqttDisconnectedState : State<
    On<MqttStartEvent, MqttConnectingState>,
    On<MqttStopEvent, MqttIdleState>,
    On<MqttErrorEvent, MqttErrorState>
> {};

struct MqttErrorState : State<
    On<MqttStartEvent, MqttIdleState>,
    On<MqttStopEvent, MqttIdleState>
> {};

// ── MqttService ─────────────────────────────────────────────────────────

/// MQTT service with state machine and signal-slot integration.
///
/// Manages MQTT client lifecycle, connects when WiFi is up,
/// disconnects when WiFi is down. Emits framework signals for
/// connection status and incoming messages.
///
/// Credentials are provided via MqttCredentials interface — the
/// caller creates a concrete credentials object and passes it
/// to the constructor. The credentials object must outlive MqttService.
///
/// Usage:
///   static MyCredentials creds(...);
///   auto* mqtt = registry.createService<embed::MqttService>(creds);
///   registry.startAll();  // connects when WiFi is up
class MqttService : public Service,
                    public StateMachine<MqttService,
                                        MqttIdleState,
                                        MqttConnectingState,
                                        MqttConnectedState,
                                        MqttDisconnectedState,
                                        MqttErrorState> {
public:
    const char* serviceName() const override { return "MqttService"; }

    /// Construct with credentials reference. Caller owns the credentials object.
    explicit MqttService(MqttCredentials& credentials);
    ~MqttService() override;

    void start() override;
    void stop() override;

    /// Framework signals
    Signal<MqttConnected> onConnected;
    Signal<MqttDisconnected> onDisconnected;
    Signal<MqttMessageReceived> onMessage;

    /// Publish a message. Returns message ID on success, -1 on failure.
    int publish(const char* topic, const char* data, int len,
                int qos = 1, bool retain = false);

    /// Subscribe to a topic. Returns message ID on success, -1 on failure.
    int subscribe(const char* topic, int qos = 1);

    /// Unsubscribe from a topic. Returns message ID on success, -1 on failure.
    int unsubscribe(const char* topic);

    /// Check if currently connected to broker.
    bool isConnected() const;

    // ── State transition handlers ──────────────────────────────────────

    void onStateChanged(const TransitionTo<MqttConnectingState>&);
    void onStateChanged(const TransitionTo<MqttConnectedState>&);
    void onStateChanged(const TransitionTo<MqttDisconnectedState>&);
    void onStateChanged(const TransitionTo<MqttIdleState>&);
    void onStateChanged(const TransitionTo<MqttErrorState>&);
    void onStateChanged(const Nothing&);

    /// Get current state name for logging.
    const char* currentStateName() const {
        return std::visit([](auto* statePtr) -> const char* {
            using S = std::decay_t<decltype(*statePtr)>;
            if constexpr (std::is_same_v<S, MqttIdleState>) return "Idle";
            else if constexpr (std::is_same_v<S, MqttConnectingState>) return "Connecting";
            else if constexpr (std::is_same_v<S, MqttConnectedState>) return "Connected";
            else if constexpr (std::is_same_v<S, MqttDisconnectedState>) return "Disconnected";
            else if constexpr (std::is_same_v<S, MqttErrorState>) return "Error";
            else return "Unknown";
        }, getCurrentState());
    }

private:
    MqttCredentials* credentials_;
    esp_mqtt_client_handle_t client_ = nullptr;
    bool mqttInitialized_ = false;
    int retryCount_ = 0;

    // Reconnect timer
    esp_timer_handle_t reconnectTimer_ = nullptr;
    static void reconnectTimerCallback(void* arg);

    // WiFi integration (slots connected in start())
    Slot<WifiConnected> wifiConnectedSlot_{onWifiConnected, this};
    Slot<WifiDisconnected> wifiDisconnectedSlot_{onWifiDisconnected, this};
    static void onWifiConnected(const WifiConnected& msg, void* ctx);
    static void onWifiDisconnected(const WifiDisconnected& msg, void* ctx);

    // ESP-IDF MQTT event handling
    static void mqttEventHandler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data);
    void processMqttEvent(int32_t event_id, void* event_data);

    // MQTT client lifecycle
    esp_err_t initMqttClient();
    void destroyMqttClient();
};

} // namespace embed
