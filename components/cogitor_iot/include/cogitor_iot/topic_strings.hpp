#pragma once

#include <string_view>

// Token macros — used for compile-time path concatenation into full_suffix.
// Prefer topic_str::* string_view aliases in normal code.
#define COGITOR_TOPIC_UP "up"
#define COGITOR_TOPIC_DOWN "down"

#define COGITOR_TOPIC_STATUS "status"
#define COGITOR_TOPIC_TELEMETRY "telemetry"
#define COGITOR_TOPIC_EVENTS "events"
#define COGITOR_TOPIC_ATTRIBUTES "attributes"
#define COGITOR_TOPIC_RPC "rpc"
#define COGITOR_TOPIC_NTP "ntp"
#define COGITOR_TOPIC_OTA "ota"
#define COGITOR_TOPIC_LOGS "logs"

#define COGITOR_TOPIC_DATA "data"
#define COGITOR_TOPIC_POST "post"
#define COGITOR_TOPIC_REPORT "report"
#define COGITOR_TOPIC_REQUEST "request"
#define COGITOR_TOPIC_RESPONSE "response"
#define COGITOR_TOPIC_UPDATE "update"
#define COGITOR_TOPIC_VERSION "version"
#define COGITOR_TOPIC_QUERY "query"
#define COGITOR_TOPIC_CANCEL "cancel"
#define COGITOR_TOPIC_PROGRESS "progress"

#define COGITOR_TOPIC_PATH2(a, b) "/" a "/" b
#define COGITOR_TOPIC_PATH3(a, b, c) "/" a "/" b "/" c

namespace cogitor::iot::topic_str {

inline constexpr std::string_view kProtocolRoot = "iot/v1";
inline constexpr std::string_view kShortRoot = "v1";
inline constexpr std::string_view kWildcard = "#";

namespace dir {
inline constexpr std::string_view kUp = COGITOR_TOPIC_UP;
inline constexpr std::string_view kDown = COGITOR_TOPIC_DOWN;
} // namespace dir

namespace cap {
inline constexpr std::string_view kStatus = COGITOR_TOPIC_STATUS;
inline constexpr std::string_view kTelemetry = COGITOR_TOPIC_TELEMETRY;
inline constexpr std::string_view kEvents = COGITOR_TOPIC_EVENTS;
inline constexpr std::string_view kAttributes = COGITOR_TOPIC_ATTRIBUTES;
inline constexpr std::string_view kRpc = COGITOR_TOPIC_RPC;
inline constexpr std::string_view kNtp = COGITOR_TOPIC_NTP;
inline constexpr std::string_view kOta = COGITOR_TOPIC_OTA;
inline constexpr std::string_view kLogs = COGITOR_TOPIC_LOGS;
} // namespace cap

namespace op {
inline constexpr std::string_view kData = COGITOR_TOPIC_DATA;
inline constexpr std::string_view kPost = COGITOR_TOPIC_POST;
inline constexpr std::string_view kReport = COGITOR_TOPIC_REPORT;
inline constexpr std::string_view kRequest = COGITOR_TOPIC_REQUEST;
inline constexpr std::string_view kResponse = COGITOR_TOPIC_RESPONSE;
inline constexpr std::string_view kUpdate = COGITOR_TOPIC_UPDATE;
inline constexpr std::string_view kVersion = COGITOR_TOPIC_VERSION;
inline constexpr std::string_view kQuery = COGITOR_TOPIC_QUERY;
inline constexpr std::string_view kCancel = COGITOR_TOPIC_CANCEL;
inline constexpr std::string_view kProgress = COGITOR_TOPIC_PROGRESS;
} // namespace op

/// Short-style absolute topics (protocol v1).
namespace short_topic {

inline constexpr std::string_view kStatus = "v1/me/s";
inline constexpr std::string_view kTelemetry = "v1/me/t";
inline constexpr std::string_view kEvents = "v1/me/e";
inline constexpr std::string_view kAttributesReport = "v1/me/a";
inline constexpr std::string_view kAttributesRequest = "v1/me/a/req";
inline constexpr std::string_view kAttributesResponse = "v1/me/a/res";
inline constexpr std::string_view kAttributesUpdate = "v1/me/a/upd";
inline constexpr std::string_view kRpcRequest = "v1/me/r/req";
inline constexpr std::string_view kRpcResponse = "v1/me/r/res";
inline constexpr std::string_view kRpcClientRequest = "v1/me/r/creq";
inline constexpr std::string_view kRpcClientResponse = "v1/me/r/cres";
inline constexpr std::string_view kNtpRequest = "v1/me/n/req";
inline constexpr std::string_view kNtpResponse = "v1/me/n/res";
inline constexpr std::string_view kOtaVersion = "v1/me/o/ver";
inline constexpr std::string_view kOtaQuery = "v1/me/o/q";
inline constexpr std::string_view kOtaUpdate = "v1/me/o/upd";
inline constexpr std::string_view kOtaCancel = "v1/me/o/can";
inline constexpr std::string_view kOtaProgress = "v1/me/o/p";
inline constexpr std::string_view kLogs = "v1/me/l";
inline constexpr std::string_view kDownstream = "v1/me/#";

} // namespace short_topic

/// Full-style path suffixes (ends-with match). Built from the same tokens as dir/cap/op.
namespace full_suffix {

inline constexpr std::string_view kStatus =
    COGITOR_TOPIC_PATH2(COGITOR_TOPIC_UP, COGITOR_TOPIC_STATUS);
inline constexpr std::string_view kTelemetry =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_TELEMETRY, COGITOR_TOPIC_DATA);
inline constexpr std::string_view kEvents =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_EVENTS, COGITOR_TOPIC_POST);
inline constexpr std::string_view kAttributesReport =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_ATTRIBUTES, COGITOR_TOPIC_REPORT);
inline constexpr std::string_view kAttributesRequest =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_ATTRIBUTES, COGITOR_TOPIC_REQUEST);
inline constexpr std::string_view kAttributesResponse =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_ATTRIBUTES, COGITOR_TOPIC_RESPONSE);
inline constexpr std::string_view kAttributesUpdate =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_ATTRIBUTES, COGITOR_TOPIC_UPDATE);
inline constexpr std::string_view kRpcRequest =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_RPC, COGITOR_TOPIC_REQUEST);
inline constexpr std::string_view kRpcResponse =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_RPC, COGITOR_TOPIC_RESPONSE);
inline constexpr std::string_view kRpcClientRequest =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_RPC, COGITOR_TOPIC_REQUEST);
inline constexpr std::string_view kRpcClientResponse =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_RPC, COGITOR_TOPIC_RESPONSE);
inline constexpr std::string_view kNtpRequest =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_NTP, COGITOR_TOPIC_REQUEST);
inline constexpr std::string_view kNtpResponse =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_NTP, COGITOR_TOPIC_RESPONSE);
inline constexpr std::string_view kOtaVersion =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_OTA, COGITOR_TOPIC_VERSION);
inline constexpr std::string_view kOtaQuery =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_OTA, COGITOR_TOPIC_QUERY);
inline constexpr std::string_view kOtaUpdate =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_OTA, COGITOR_TOPIC_UPDATE);
inline constexpr std::string_view kOtaCancel =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_DOWN, COGITOR_TOPIC_OTA, COGITOR_TOPIC_CANCEL);
inline constexpr std::string_view kOtaProgress =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_OTA, COGITOR_TOPIC_PROGRESS);
inline constexpr std::string_view kLogs =
    COGITOR_TOPIC_PATH3(COGITOR_TOPIC_UP, COGITOR_TOPIC_LOGS, COGITOR_TOPIC_REPORT);
inline constexpr std::string_view kUpSegment = "/" COGITOR_TOPIC_UP "/";

} // namespace full_suffix

} // namespace cogitor::iot::topic_str

// Convenience aliases at cogitor::iot level (same names as before).
namespace cogitor::iot {
namespace short_topic = topic_str::short_topic;
namespace full_suffix = topic_str::full_suffix;
} // namespace cogitor::iot
