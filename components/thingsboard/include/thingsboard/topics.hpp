#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace thingsboard {

/// Topic naming style (ThingsBoard 3.5+ short topics vs classic v1).
enum class TopicStyle : uint8_t {
    Short,     ///< v2/...
    Standard,  ///< v1/devices/me/...
};

/// Build MQTT topic strings for ThingsBoard device API.
class Topics {
public:
    explicit Topics(TopicStyle style = TopicStyle::Short) : style_(style) {}

    TopicStyle style() const { return style_; }

    std::string telemetry() const;
    std::string attributes() const;
    std::string attributesRequest(uint32_t requestId) const;
    std::string attributesResponseSubscribe() const;
    std::string rpcRequestSubscribe() const;
    std::string rpcResponse(uint32_t requestId) const;
    std::string claim() const;

    /// Extract trailing numeric id from .../req/$id or .../res/$id topics.
    static uint32_t parseTrailingId(std::string_view topic);

    /// Extract RPC request id from topic, or 0 if not an RPC request.
    static uint32_t parseRpcRequestId(std::string_view topic);

    /// True if topic is a shared-attribute push (not attr response / RPC).
    static bool isAttributeUpdate(std::string_view topic);

    /// True if topic is a server-side RPC request.
    static bool isRpcRequest(std::string_view topic);

    /// True if topic is an attribute request response.
    static bool isAttributeResponse(std::string_view topic);

private:
    TopicStyle style_;
};

} // namespace thingsboard
