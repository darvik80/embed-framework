#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_service.hpp"
#include "crearts_iot/topics.hpp"
#include "crearts_iot/telemetry.hpp"
#include "crearts_iot/attributes.hpp"
#include "crearts_iot/credentials.hpp"
#include "crearts_iot/rpc_registry.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace crearts::iot {

struct AttributeUpdate {
    embed::string<767> payload;
};
static_assert(embed::Message<AttributeUpdate>);

struct AttributeResponse {
    uint32_t requestId = 0;
    embed::string<767> payload;
};
static_assert(embed::Message<AttributeResponse>);

struct RpcRequest {
    uint32_t requestId = 0;
    embed::string<63> method;
    embed::string<1199> params;
};
static_assert(embed::Message<RpcRequest>);

struct NtpResponse {
    uint32_t requestId = 0;
    int64_t deviceSendTime = 0;
    int64_t serverRecvTime = 0;
    int64_t serverSendTime = 0;
};
static_assert(embed::Message<NtpResponse>);

struct OtaUpdate {
    embed::string<1400> payload;
};
static_assert(embed::Message<OtaUpdate>);

struct OtaCancel {
    embed::string<255> payload;
};
static_assert(embed::Message<OtaCancel>);

/// Crearts IoT device API over embed::MqttService (protocol v1).
///
/// On MQTT connect: subscribe downstream only.
/// Presence: platform detects online from the MQTT session; offline via LWT
/// on the status topic (configured in CreartsCredentials).
/// Correlation for RPC / attributes / NTP uses JSON field `"id"`.
class CreartsIotService : public embed::Service {
public:
    explicit CreartsIotService(const CreartsCredentials& credentials);

    const char* serviceName() const override { return "CreartsIotService"; }

    void start() override;
    void stop() override;

    int publishTelemetry(std::string_view json, int qos = 1);
    int publishTelemetry(const TelemetryBuilder& builder, int qos = 1);
    int publishTelemetry(const TelemetryBatch& batch, int qos = 1);

    int publishEvents(std::string_view json, int qos = 1);

    int publishAttributes(std::string_view json, int qos = 1);
    int publishAttributes(const AttributeBuilder& builder, int qos = 1);

    int requestAttributes(const AttributeRequestBuilder& request, int qos = 1);
    int requestAttributes(const AttributeRequestBuilder& request,
                          uint32_t& outRequestId,
                          int qos = 1);

    int respondRpc(uint32_t requestId,
                   int code,
                   std::string_view message,
                   std::string_view dataJson = {},
                   int qos = 1);

    int requestNtp(uint32_t& outRequestId, int64_t deviceSendTimeMs, int qos = 1);

    int publishOtaVersion(std::string_view version, std::string_view module = "main", int qos = 1);
    int publishOtaQuery(std::string_view module = "main",
                        std::string_view version = {},
                        int qos = 1);
    int publishOtaProgress(std::string_view module, int step, std::string_view desc, int qos = 1);

    int publishLogs(std::string_view json, int qos = 0);

    [[nodiscard]] const CreartsCredentials& credentials() const { return *credentials_; }

    /// Firmware RPC catalog. Register handlers here; `rpc-list` is built-in.
    [[nodiscard]] RpcRegistry& rpc() { return *rpc_; }
    [[nodiscard]] const RpcRegistry& rpc() const { return *rpc_; }

    embed::Signal<AttributeUpdate> onAttributeUpdate;
    embed::Signal<AttributeResponse> onAttributeResponse;
    embed::Signal<RpcRequest> onRpcRequest;
    embed::Signal<NtpResponse> onNtpResponse;
    embed::Signal<OtaUpdate> onOtaUpdate;
    embed::Signal<OtaCancel> onOtaCancel;

private:
    const CreartsCredentials* credentials_ = nullptr;
    embed::MqttService* mqtt_ = nullptr;
    Topics topics_;
    std::unique_ptr<RpcRegistry> rpc_;
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
    int publishRaw(const std::string& topic, std::string_view json, int qos, bool retain = false);
};

} // namespace crearts::iot
