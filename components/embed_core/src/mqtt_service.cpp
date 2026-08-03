#include "embed_core/mqtt_service.hpp"

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

namespace embed {

static const char* TAG = "MqttService";

MqttService::MqttService(MqttCredentials& credentials)
    : credentials_(&credentials) {}

MqttService::~MqttService() {
    stop();
}

void MqttService::start() {
    ESP_LOGI(TAG, "Starting MqttService, waiting for WiFi...");

    // Connect to WifiService signals
    auto& reg = ServiceRegistry::instance();
    auto* wifi = reg.getService<WifiService>();

    if (wifi) {
        wifiConnectedSlot_.connect(wifi->onConnected);
        wifiDisconnectedSlot_.connect(wifi->onDisconnected);
        ESP_LOGI(TAG, "Connected to WifiService signals");
    } else {
        ESP_LOGW(TAG, "WifiService not found — MQTT will not auto-connect");
    }
}

void MqttService::stop() {
    if (reconnectTimer_) {
        esp_timer_stop(reconnectTimer_);
        esp_timer_delete(reconnectTimer_);
        reconnectTimer_ = nullptr;
    }

    destroyMqttClient();

    wifiConnectedSlot_.disconnect();
    wifiDisconnectedSlot_.disconnect();
}

// ── WiFi signal handlers ────────────────────────────────────────────────

void MqttService::onWifiConnected(const WifiConnected& /*msg*/, void* ctx) {
    auto* self = static_cast<MqttService*>(ctx);
    ESP_LOGI(TAG, "WiFi connected, starting MQTT...");
    self->handle(MqttStartEvent{});
}

void MqttService::onWifiDisconnected(const WifiDisconnected& /*msg*/, void* ctx) {
    auto* self = static_cast<MqttService*>(ctx);
    ESP_LOGW(TAG, "WiFi disconnected, stopping MQTT");
    self->handle(MqttStopEvent{});
}

// ── State transition handlers ───────────────────────────────────────────

void MqttService::onStateChanged(const TransitionTo<MqttConnectingState>&) {
    ESP_LOGI("MqttSM", "[%s] -> Connecting", currentStateName());

    if (!mqttInitialized_) {
        esp_err_t ret = initMqttClient();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init MQTT client: %s", esp_err_to_name(ret));
            handle(MqttErrorEvent{.error = ret});
            return;
        }
    }

    if (client_) {
        esp_err_t ret = esp_mqtt_client_start(client_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
            handle(MqttErrorEvent{.error = ret});
        }
    }
}

void MqttService::onStateChanged(const TransitionTo<MqttConnectedState>&) {
    retryCount_ = 0;
    ESP_LOGI("MqttSM", "[%s] -> Connected", currentStateName());

    MqttConnected msg;
    if (credentials_) {
        msg.brokerUri = credentials_->brokerUri();
    }
    onConnected.emit(msg);
}

void MqttService::onStateChanged(const TransitionTo<MqttDisconnectedState>&) {
    retryCount_++;
    ESP_LOGI("MqttSM", "[%s] -> Disconnected (retry %d/%d)",
             currentStateName(), retryCount_, CONFIG_EMBED_MQTT_MAX_RETRY);

    if (std::holds_alternative<MqttConnectedState*>(getPrevState())) {
        onDisconnected.emit(MqttDisconnected{});
    }

    if (retryCount_ <= CONFIG_EMBED_MQTT_MAX_RETRY) {
        ESP_LOGI(TAG, "Scheduling reconnect in %dms", CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS);

        if (!reconnectTimer_) {
            const esp_timer_create_args_t args = {
                .callback = reconnectTimerCallback,
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "mqtt_reconnect",
            };
            esp_timer_create(&args, &reconnectTimer_);
        }
        esp_timer_start_once(reconnectTimer_, CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS * 1000);
    } else {
        ESP_LOGE(TAG, "Max retries (%d) reached", CONFIG_EMBED_MQTT_MAX_RETRY);
        handle(MqttErrorEvent{.error = ESP_ERR_TIMEOUT});
    }
}

void MqttService::onStateChanged(const TransitionTo<MqttIdleState>&) {
    ESP_LOGI("MqttSM", "[%s] -> Idle", currentStateName());
    destroyMqttClient();
}

void MqttService::onStateChanged(const TransitionTo<MqttErrorState>&) {
    ESP_LOGI("MqttSM", "[%s] -> Error", currentStateName());
    if (std::holds_alternative<MqttConnectedState*>(getPrevState())) {
        onDisconnected.emit(MqttDisconnected{});
    }
    destroyMqttClient();
}

void MqttService::onStateChanged(const Nothing&) {
    ESP_LOGD("MqttSM", "[%s] no transition", currentStateName());
}

// ── Reconnect timer ─────────────────────────────────────────────────────

void MqttService::reconnectTimerCallback(void* arg) {
    auto* self = static_cast<MqttService*>(arg);
    ESP_LOGI(TAG, "Reconnect timer fired, attempting connection...");
    self->handle(MqttStartEvent{});
}

// ── MQTT client lifecycle ───────────────────────────────────────────────

esp_err_t MqttService::initMqttClient() {
    if (!credentials_) {
        ESP_LOGE(TAG, "No credentials set");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mqtt_client_config_t mqttCfg = {};

    mqttCfg.broker.address.uri = credentials_->brokerUri();
    mqttCfg.credentials.username = credentials_->username();
    mqttCfg.credentials.client_id = credentials_->clientId();
    mqttCfg.credentials.authentication.password = credentials_->password();
    // Reconnect policy is owned by MqttService state machine + esp_timer
    // (CONFIG_EMBED_MQTT_MAX_RETRY / CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS).
    // Disable esp-mqtt auto-reconnect to avoid dual competing retry loops.
    mqttCfg.network.disable_auto_reconnect = true;
    mqttCfg.network.timeout_ms = 5000;
    mqttCfg.session.keepalive = CONFIG_EMBED_MQTT_KEEPALIVE;

    if (credentials_->willTopic() && credentials_->willMessage()) {
        mqttCfg.session.last_will.topic = credentials_->willTopic();
        mqttCfg.session.last_will.msg = credentials_->willMessage();
        size_t willLen = credentials_->willMessageLen();
        mqttCfg.session.last_will.msg_len =
            willLen > 0 ? static_cast<int>(willLen)
                        : static_cast<int>(std::strlen(credentials_->willMessage()));
        mqttCfg.session.last_will.qos = credentials_->willQos();
        mqttCfg.session.last_will.retain = credentials_->willRetain();
    }

    // TLS: use the built-in Mozilla CA bundle for wss:// and mqtts:// URIs.
    // A custom cert() can override this for self-signed / private CA setups.
    const char* uri = credentials_->brokerUri();
    bool isTls = uri && (strncmp(uri, "wss://", 6) == 0 || strncmp(uri, "mqtts://", 8) == 0);
    if (isTls) {
        if (credentials_->cert() != nullptr) {
            mqttCfg.broker.verification.certificate = credentials_->cert();
            size_t len = credentials_->certLen();
            mqttCfg.broker.verification.certificate_len =
                (len > 0) ? len : (std::strlen(credentials_->cert()) + 1);
        } else {
            mqttCfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        }
    }

    client_ = esp_mqtt_client_init(&mqttCfg);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_mqtt_client_register_event(
        client_, MQTT_EVENT_ANY, &mqttEventHandler, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return ret;
    }

    mqttInitialized_ = true;
    ESP_LOGI(TAG, "MQTT client initialized (uri: %s)", credentials_->brokerUri());
    return ESP_OK;
}

void MqttService::destroyMqttClient() {
    if (client_) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
    mqttInitialized_ = false;
}

// ── MQTT event handler ─────────────────────────────────────────────────

void MqttService::mqttEventHandler(void* arg, esp_event_base_t /*base*/,
                                   int32_t event_id, void* event_data) {
    auto* self = static_cast<MqttService*>(arg);
    self->processMqttEvent(event_id, event_data);
}

void MqttService::processMqttEvent(int32_t event_id, void* event_data) {
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to broker");
        handle(MqttBrokerConnectedEvent{});
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected from broker");
        handle(MqttBrokerDisconnectedEvent{});
        break;

    case MQTT_EVENT_DATA: {
        MqttMessageReceived msg;
        if (event->topic && event->topic_len > 0) {
            msg.topic.assign(event->topic, event->topic_len);
        }
        if (event->data && event->data_len > 0) {
            msg.payload.assign(event->data, event->data_len);
            if (static_cast<size_t>(event->data_len) > msg.payload.capacity()) {
                ESP_LOGW(TAG, "MQTT payload truncated: %d -> %zu bytes (topic=%s)",
                         event->data_len, msg.payload.capacity(), msg.topic.c_str());
            }
        }
        ESP_LOGD(TAG, "MQTT data: topic=%s len=%d", msg.topic.c_str(), event->data_len);
        onMessage.emit(msg);
        break;
    }

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT published, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error event");
        if (event->error_handle) {
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Transport error: esp_tls=%d, tls_stack=%d, sock_errno=%d",
                         event->error_handle->esp_tls_last_esp_err,
                         event->error_handle->esp_tls_stack_err,
                         event->error_handle->esp_transport_sock_errno);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused: 0x%x",
                         event->error_handle->connect_return_code);
            }
        }
        break;

    default:
        ESP_LOGD(TAG, "Unhandled MQTT event: %ld", event_id);
        break;
    }
}

// ── Public API ──────────────────────────────────────────────────────────

int MqttService::publish(const char* topic, const char* data, int len,
                         int qos, bool retain) {
    if (!client_) {
        ESP_LOGE(TAG, "MQTT client not available");
        return -1;
    }
    int msgId = esp_mqtt_client_publish(client_, topic, data, len, qos, retain);
    if (msgId < 0) {
        ESP_LOGE(TAG, "Failed to publish to %s", topic);
    }
    return msgId;
}

int MqttService::subscribe(const char* topic, int qos) {
    if (!client_) {
        ESP_LOGE(TAG, "MQTT client not available");
        return -1;
    }
    int msgId = esp_mqtt_client_subscribe(client_, topic, qos);
    if (msgId < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to %s", topic);
    }
    return msgId;
}

int MqttService::unsubscribe(const char* topic) {
    if (!client_) {
        ESP_LOGE(TAG, "MQTT client not available");
        return -1;
    }
    int msgId = esp_mqtt_client_unsubscribe(client_, topic);
    if (msgId < 0) {
        ESP_LOGE(TAG, "Failed to unsubscribe from %s", topic);
    }
    return msgId;
}

bool MqttService::isConnected() const {
    return std::holds_alternative<MqttConnectedState*>(getCurrentState());
}

} // namespace embed
