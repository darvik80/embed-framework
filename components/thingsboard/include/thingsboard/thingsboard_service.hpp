#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_service.hpp"
#include "thingsboard/topics.hpp"
#include "thingsboard/telemetry.hpp"
#include "thingsboard/attributes.hpp"
#include <cstdint>
#include <string_view>

namespace thingsboard {

/// Shared attribute update pushed by the server (raw JSON payload).
/// Parse with parseAttributeUpdate(payload).
struct AttributeUpdate {
    embed::string<767> payload;
};
static_assert(embed::Message<AttributeUpdate>);

/// Server-side RPC request delivered to the device.
struct RpcRequest {
    uint32_t requestId = 0;
    embed::string<63> method;
    embed::string<511> params;
};
static_assert(embed::Message<RpcRequest>);

/// Attribute request/response correlation payload (raw JSON).
/// Parse with parseAttributeResponse(payload).
struct AttributeResponse {
    uint32_t requestId = 0;
    embed::string<767> payload;
};
static_assert(embed::Message<AttributeResponse>);

/// ThingsBoard device API over embed::MqttService.
///
/// Telemetry: https://thingsboard.io/docs/reference/mqtt-api/telemetry/
/// Attributes: https://thingsboard.io/docs/reference/mqtt-api/attributes/
class ThingsBoardService : public embed::Service {
public:
    explicit ThingsBoardService(TopicStyle style = TopicStyle::Short);

    const char* serviceName() const override { return "ThingsBoardService"; }

    void start() override;
    void stop() override;

    int publishTelemetry(std::string_view json, int qos = 1);
    int publishTelemetry(const TelemetryBuilder& builder, int qos = 1);
    int publishTelemetry(const TelemetryBatch& batch, int qos = 1);

    int publishAttributes(std::string_view json, int qos = 1);
    int publishAttributes(const AttributeBuilder& builder, int qos = 1);

    int requestAttributes(std::string_view keysJson, int qos = 1);
    int requestAttributes(const AttributeRequestBuilder& request, int qos = 1);
    int requestAttributes(const AttributeRequestBuilder& request, uint32_t& outRequestId, int qos = 1);

    int respondRpc(uint32_t requestId, std::string_view jsonPayload, int qos = 1);

    embed::Signal<AttributeUpdate> onAttributeUpdate;
    embed::Signal<RpcRequest> onRpcRequest;
    embed::Signal<AttributeResponse> onAttributeResponse;

private:
    embed::MqttService* mqtt_ = nullptr;
    Topics topics_;
    uint32_t nextRequestId_ = 1;
    bool subscribed_ = false;

    embed::Slot<embed::MqttConnected> mqttConnectedSlot_{onMqttConnected, this};
    embed::Slot<embed::MqttDisconnected> mqttDisconnectedSlot_{onMqttDisconnected, this};
    embed::Slot<embed::MqttMessageReceived> mqttMessageSlot_{onMqttMessage, this};

    static void onMqttConnected(const embed::MqttConnected& msg, void* ctx);
    static void onMqttDisconnected(const embed::MqttDisconnected& msg, void* ctx);
    static void onMqttMessage(const embed::MqttMessageReceived& msg, void* ctx);

    void subscribeAll();
    void unsubscribeAll();
    void handleMessage(std::string_view topic, std::string_view payload);

    uint32_t allocRequestId();
};

} // namespace thingsboard
