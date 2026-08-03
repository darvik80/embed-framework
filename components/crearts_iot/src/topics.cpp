#include "crearts_iot/topics.hpp"

#include <format>

namespace crearts::iot {

namespace {

bool endsWith(std::string_view topic, std::string_view suffix)
{
    return topic.size() >= suffix.size() &&
           topic.compare(topic.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

Topics::Topics(std::string_view productId, std::string_view deviceId, TopicStyle style)
    : productId_(productId)
    , deviceId_(deviceId)
    , style_(style)
{}

std::string Topics::full(const char* direction,
                         const char* capability,
                         const char* operation) const
{
    return std::format("iot/v1/{}/{}/{}/{}/{}",
                       productId_, deviceId_, direction, capability, operation);
}

std::string Topics::statusPublish() const
{
    if (style_ == TopicStyle::Short) {
        return "v1/s";
    }
    return std::format("iot/v1/{}/{}/up/status", productId_, deviceId_);
}

std::string Topics::telemetryPublish() const
{
    return style_ == TopicStyle::Short ? "v1/t" : full("up", "telemetry", "data");
}

std::string Topics::eventsPost() const
{
    return style_ == TopicStyle::Short ? "v1/e" : full("up", "events", "post");
}

std::string Topics::attributesReport() const
{
    return style_ == TopicStyle::Short ? "v1/a" : full("up", "attributes", "report");
}

std::string Topics::attributesRequest() const
{
    return style_ == TopicStyle::Short ? "v1/a/req" : full("up", "attributes", "request");
}

std::string Topics::attributesResponseSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/a/res" : full("down", "attributes", "response");
}

std::string Topics::attributesUpdateSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/a/upd" : full("down", "attributes", "update");
}

std::string Topics::rpcRequestSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/r/req" : full("down", "rpc", "request");
}

std::string Topics::rpcResponse() const
{
    return style_ == TopicStyle::Short ? "v1/r/res" : full("up", "rpc", "response");
}

std::string Topics::rpcClientRequest() const
{
    return style_ == TopicStyle::Short ? "v1/r/creq" : full("up", "rpc", "request");
}

std::string Topics::rpcClientResponseSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/r/cres" : full("down", "rpc", "response");
}

std::string Topics::ntpRequest() const
{
    return style_ == TopicStyle::Short ? "v1/n/req" : full("up", "ntp", "request");
}

std::string Topics::ntpResponseSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/n/res" : full("down", "ntp", "response");
}

std::string Topics::otaVersion() const
{
    return style_ == TopicStyle::Short ? "v1/o/ver" : full("up", "ota", "version");
}

std::string Topics::otaQuery() const
{
    return style_ == TopicStyle::Short ? "v1/o/q" : full("up", "ota", "query");
}

std::string Topics::otaUpdateSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/o/upd" : full("down", "ota", "update");
}

std::string Topics::otaCancelSubscribe() const
{
    return style_ == TopicStyle::Short ? "v1/o/can" : full("down", "ota", "cancel");
}

std::string Topics::otaProgress() const
{
    return style_ == TopicStyle::Short ? "v1/o/p" : full("up", "ota", "progress");
}

std::string Topics::logsReport() const
{
    return style_ == TopicStyle::Short ? "v1/l" : full("up", "logs", "report");
}

std::string Topics::downstreamSubscribe() const
{
    if (style_ == TopicStyle::Short) {
        return "v1/#";
    }
    return std::format("iot/v1/{}/{}/down/#", productId_, deviceId_);
}

bool Topics::isRpcRequest(std::string_view topic)
{
    return topic == "v1/r/req" || endsWith(topic, "/down/rpc/request");
}

bool Topics::isAttributeUpdate(std::string_view topic)
{
    return topic == "v1/a/upd" || endsWith(topic, "/down/attributes/update");
}

bool Topics::isAttributeResponse(std::string_view topic)
{
    return topic == "v1/a/res" || endsWith(topic, "/down/attributes/response");
}

bool Topics::isNtpResponse(std::string_view topic)
{
    return topic == "v1/n/res" || endsWith(topic, "/down/ntp/response");
}

bool Topics::isOtaUpdate(std::string_view topic)
{
    return topic == "v1/o/upd" || endsWith(topic, "/down/ota/update");
}

bool Topics::isOtaCancel(std::string_view topic)
{
    return topic == "v1/o/can" || endsWith(topic, "/down/ota/cancel");
}

bool Topics::isRpcClientResponse(std::string_view topic)
{
    return topic == "v1/r/cres" || endsWith(topic, "/down/rpc/response");
}

} // namespace crearts::iot
