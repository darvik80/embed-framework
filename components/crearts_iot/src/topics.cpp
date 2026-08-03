#include "crearts_iot/topics.hpp"

#include <format>

namespace crearts::iot {

namespace {

bool endsWith(std::string_view topic, std::string_view suffix)
{
    return topic.size() >= suffix.size() &&
           topic.compare(topic.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string pick(TopicStyle style, std::string_view shortTopic, std::string fullTopic)
{
    return style == TopicStyle::Short ? std::string(shortTopic) : std::move(fullTopic);
}

} // namespace

Topics::Topics(std::string_view productId, std::string_view deviceId, TopicStyle style)
    : productId_(productId)
    , deviceId_(deviceId)
    , style_(style)
{}

std::string Topics::full(std::string_view direction,
                         std::string_view capability,
                         std::string_view operation) const
{
    return std::format("{}/{}/{}/{}/{}/{}",
                       topic_str::kProtocolRoot,
                       productId_,
                       deviceId_,
                       direction,
                       capability,
                       operation);
}

std::string Topics::fullNoOp(std::string_view direction, std::string_view capability) const
{
    return std::format("{}/{}/{}/{}/{}",
                       topic_str::kProtocolRoot,
                       productId_,
                       deviceId_,
                       direction,
                       capability);
}

std::string Topics::statusPublish() const
{
    return pick(style_,
                short_topic::kStatus,
                fullNoOp(topic_str::dir::kUp, topic_str::cap::kStatus));
}

std::string Topics::telemetryPublish() const
{
    return pick(style_,
                short_topic::kTelemetry,
                full(topic_str::dir::kUp, topic_str::cap::kTelemetry, topic_str::op::kData));
}

std::string Topics::eventsPost() const
{
    return pick(style_,
                short_topic::kEvents,
                full(topic_str::dir::kUp, topic_str::cap::kEvents, topic_str::op::kPost));
}

std::string Topics::attributesReport() const
{
    return pick(style_,
                short_topic::kAttributesReport,
                full(topic_str::dir::kUp, topic_str::cap::kAttributes, topic_str::op::kReport));
}

std::string Topics::attributesRequest() const
{
    return pick(style_,
                short_topic::kAttributesRequest,
                full(topic_str::dir::kUp, topic_str::cap::kAttributes, topic_str::op::kRequest));
}

std::string Topics::attributesResponseSubscribe() const
{
    return pick(style_,
                short_topic::kAttributesResponse,
                full(topic_str::dir::kDown, topic_str::cap::kAttributes, topic_str::op::kResponse));
}

std::string Topics::attributesUpdateSubscribe() const
{
    return pick(style_,
                short_topic::kAttributesUpdate,
                full(topic_str::dir::kDown, topic_str::cap::kAttributes, topic_str::op::kUpdate));
}

std::string Topics::rpcRequestSubscribe() const
{
    return pick(style_,
                short_topic::kRpcRequest,
                full(topic_str::dir::kDown, topic_str::cap::kRpc, topic_str::op::kRequest));
}

std::string Topics::rpcResponse() const
{
    return pick(style_,
                short_topic::kRpcResponse,
                full(topic_str::dir::kUp, topic_str::cap::kRpc, topic_str::op::kResponse));
}

std::string Topics::rpcClientRequest() const
{
    return pick(style_,
                short_topic::kRpcClientRequest,
                full(topic_str::dir::kUp, topic_str::cap::kRpc, topic_str::op::kRequest));
}

std::string Topics::rpcClientResponseSubscribe() const
{
    return pick(style_,
                short_topic::kRpcClientResponse,
                full(topic_str::dir::kDown, topic_str::cap::kRpc, topic_str::op::kResponse));
}

std::string Topics::ntpRequest() const
{
    return pick(style_,
                short_topic::kNtpRequest,
                full(topic_str::dir::kUp, topic_str::cap::kNtp, topic_str::op::kRequest));
}

std::string Topics::ntpResponseSubscribe() const
{
    return pick(style_,
                short_topic::kNtpResponse,
                full(topic_str::dir::kDown, topic_str::cap::kNtp, topic_str::op::kResponse));
}

std::string Topics::otaVersion() const
{
    return pick(style_,
                short_topic::kOtaVersion,
                full(topic_str::dir::kUp, topic_str::cap::kOta, topic_str::op::kVersion));
}

std::string Topics::otaQuery() const
{
    return pick(style_,
                short_topic::kOtaQuery,
                full(topic_str::dir::kUp, topic_str::cap::kOta, topic_str::op::kQuery));
}

std::string Topics::otaUpdateSubscribe() const
{
    return pick(style_,
                short_topic::kOtaUpdate,
                full(topic_str::dir::kDown, topic_str::cap::kOta, topic_str::op::kUpdate));
}

std::string Topics::otaCancelSubscribe() const
{
    return pick(style_,
                short_topic::kOtaCancel,
                full(topic_str::dir::kDown, topic_str::cap::kOta, topic_str::op::kCancel));
}

std::string Topics::otaProgress() const
{
    return pick(style_,
                short_topic::kOtaProgress,
                full(topic_str::dir::kUp, topic_str::cap::kOta, topic_str::op::kProgress));
}

std::string Topics::logsReport() const
{
    return pick(style_,
                short_topic::kLogs,
                full(topic_str::dir::kUp, topic_str::cap::kLogs, topic_str::op::kReport));
}

std::string Topics::downstreamSubscribe() const
{
    if (style_ == TopicStyle::Short) {
        return std::string(short_topic::kDownstream);
    }
    return std::format("{}/{}/{}/{}/{}",
                       topic_str::kProtocolRoot,
                       productId_,
                       deviceId_,
                       topic_str::dir::kDown,
                       topic_str::kWildcard);
}

bool Topics::isUplink(std::string_view topic)
{
    if (topic.find(full_suffix::kUpSegment) != std::string_view::npos) {
        return true;
    }
    return topic == short_topic::kStatus || topic == short_topic::kTelemetry ||
           topic == short_topic::kEvents || topic == short_topic::kAttributesReport ||
           topic == short_topic::kAttributesRequest || topic == short_topic::kRpcResponse ||
           topic == short_topic::kRpcClientRequest || topic == short_topic::kNtpRequest ||
           topic == short_topic::kOtaVersion || topic == short_topic::kOtaQuery ||
           topic == short_topic::kOtaProgress || topic == short_topic::kLogs;
}

bool Topics::isRpcRequest(std::string_view topic)
{
    return topic == short_topic::kRpcRequest || endsWith(topic, full_suffix::kRpcRequest);
}

bool Topics::isAttributeUpdate(std::string_view topic)
{
    return topic == short_topic::kAttributesUpdate ||
           endsWith(topic, full_suffix::kAttributesUpdate);
}

bool Topics::isAttributeResponse(std::string_view topic)
{
    return topic == short_topic::kAttributesResponse ||
           endsWith(topic, full_suffix::kAttributesResponse);
}

bool Topics::isNtpResponse(std::string_view topic)
{
    return topic == short_topic::kNtpResponse || endsWith(topic, full_suffix::kNtpResponse);
}

bool Topics::isOtaUpdate(std::string_view topic)
{
    return topic == short_topic::kOtaUpdate || endsWith(topic, full_suffix::kOtaUpdate);
}

bool Topics::isOtaCancel(std::string_view topic)
{
    return topic == short_topic::kOtaCancel || endsWith(topic, full_suffix::kOtaCancel);
}

bool Topics::isRpcClientResponse(std::string_view topic)
{
    return topic == short_topic::kRpcClientResponse ||
           endsWith(topic, full_suffix::kRpcClientResponse);
}

} // namespace crearts::iot
