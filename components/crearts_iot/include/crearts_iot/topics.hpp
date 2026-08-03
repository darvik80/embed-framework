#pragma once

#include "crearts_iot/topic_strings.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace crearts::iot {

/// Topic naming style (see docs/iot-platform-mqtt-spec.md).
enum class TopicStyle : uint8_t {
    Short, ///< v1/t, v1/a, v1/r/req, ...
    Full,  ///< iot/v1/{product}/{device}/up|down/...
};

/// Build MQTT topic strings for Crearts IoT protocol v1.
///
/// productId / deviceId are non-owning views — the backing storage
/// (typically CreartsCredentials) must outlive Topics.
///
/// String literals live in topic_strings.hpp (`topic_str::dir/cap/op`,
/// `short_topic`, `full_suffix`).
class Topics {
public:
    Topics(std::string_view productId,
           std::string_view deviceId,
           TopicStyle style = TopicStyle::Short);

    [[nodiscard]] TopicStyle style() const { return style_; }
    [[nodiscard]] std::string_view productId() const { return productId_; }
    [[nodiscard]] std::string_view deviceId() const { return deviceId_; }

    [[nodiscard]] std::string statusPublish() const;
    [[nodiscard]] std::string telemetryPublish() const;
    [[nodiscard]] std::string eventsPost() const;
    [[nodiscard]] std::string attributesReport() const;
    [[nodiscard]] std::string attributesRequest() const;
    [[nodiscard]] std::string attributesResponseSubscribe() const;
    [[nodiscard]] std::string attributesUpdateSubscribe() const;
    [[nodiscard]] std::string rpcRequestSubscribe() const;
    [[nodiscard]] std::string rpcResponse() const;
    [[nodiscard]] std::string rpcClientRequest() const;
    [[nodiscard]] std::string rpcClientResponseSubscribe() const;
    [[nodiscard]] std::string ntpRequest() const;
    [[nodiscard]] std::string ntpResponseSubscribe() const;
    [[nodiscard]] std::string otaVersion() const;
    [[nodiscard]] std::string otaQuery() const;
    [[nodiscard]] std::string otaUpdateSubscribe() const;
    [[nodiscard]] std::string otaCancelSubscribe() const;
    [[nodiscard]] std::string otaProgress() const;
    [[nodiscard]] std::string logsReport() const;
    [[nodiscard]] std::string downstreamSubscribe() const;

    /// True for device→server topics (ignore when subscribed to short `v1/#`).
    [[nodiscard]] static bool isUplink(std::string_view topic);

    [[nodiscard]] static bool isRpcRequest(std::string_view topic);
    [[nodiscard]] static bool isAttributeUpdate(std::string_view topic);
    [[nodiscard]] static bool isAttributeResponse(std::string_view topic);
    [[nodiscard]] static bool isNtpResponse(std::string_view topic);
    [[nodiscard]] static bool isOtaUpdate(std::string_view topic);
    [[nodiscard]] static bool isOtaCancel(std::string_view topic);
    [[nodiscard]] static bool isRpcClientResponse(std::string_view topic);

private:
    std::string_view productId_;
    std::string_view deviceId_;
    TopicStyle style_;

    [[nodiscard]] std::string full(std::string_view direction,
                                   std::string_view capability,
                                   std::string_view operation) const;
    [[nodiscard]] std::string fullNoOp(std::string_view direction,
                                       std::string_view capability) const;
};

} // namespace crearts::iot
