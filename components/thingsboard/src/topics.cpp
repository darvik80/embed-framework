#include "thingsboard/topics.hpp"

#include <charconv>
#include <format>

namespace thingsboard {

std::string Topics::telemetry() const
{
    return style_ == TopicStyle::Short ? "v2/t" : "v1/devices/me/telemetry";
}

std::string Topics::attributes() const
{
    return style_ == TopicStyle::Short ? "v2/a" : "v1/devices/me/attributes";
}

std::string Topics::attributesRequest(uint32_t requestId) const
{
    if (style_ == TopicStyle::Short) {
        return std::format("v2/a/req/{}", requestId);
    }
    return std::format("v1/devices/me/attributes/request/{}", requestId);
}

std::string Topics::attributesResponseSubscribe() const
{
    return style_ == TopicStyle::Short ? "v2/a/res/+"
                                       : "v1/devices/me/attributes/response/+";
}

std::string Topics::rpcRequestSubscribe() const
{
    return style_ == TopicStyle::Short ? "v2/r/req/+"
                                       : "v1/devices/me/rpc/request/+";
}

std::string Topics::rpcResponse(uint32_t requestId) const
{
    if (style_ == TopicStyle::Short) {
        return std::format("v2/r/res/{}", requestId);
    }
    return std::format("v1/devices/me/rpc/response/{}", requestId);
}

std::string Topics::claim() const
{
    return style_ == TopicStyle::Short ? "v2/c" : "v1/devices/me/claim";
}

uint32_t Topics::parseTrailingId(std::string_view topic)
{
    const auto pos = topic.find_last_of('/');
    if (pos == std::string_view::npos || pos + 1 >= topic.size()) {
        return 0;
    }
    std::string_view idPart = topic.substr(pos + 1);
    uint32_t id = 0;
    auto [ptr, ec] = std::from_chars(idPart.data(), idPart.data() + idPart.size(), id);
    if (ec != std::errc{} || ptr != idPart.data() + idPart.size()) {
        return 0;
    }
    return id;
}

uint32_t Topics::parseRpcRequestId(std::string_view topic)
{
    if (topic.starts_with("v2/r/req/") ||
        topic.find("/rpc/request/") != std::string_view::npos) {
        return parseTrailingId(topic);
    }
    return 0;
}

bool Topics::isAttributeUpdate(std::string_view topic)
{
    return topic == "v2/a" || topic == "v1/devices/me/attributes";
}

bool Topics::isRpcRequest(std::string_view topic)
{
    return topic.starts_with("v2/r/req/") ||
           topic.find("/rpc/request/") != std::string_view::npos;
}

bool Topics::isAttributeResponse(std::string_view topic)
{
    return topic.starts_with("v2/a/res/") ||
           topic.find("/attributes/response/") != std::string_view::npos;
}

} // namespace thingsboard
