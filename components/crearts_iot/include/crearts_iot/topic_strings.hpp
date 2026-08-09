#pragma once

#include <string_view>

// Token macros — used for compile-time path concatenation into full_suffix.
// Prefer topic_str::* string_view aliases in normal code.
#define CREARTS_TOPIC_UP "up"
#define CREARTS_TOPIC_DOWN "down"

#define CREARTS_TOPIC_STATUS "status"
#define CREARTS_TOPIC_TELEMETRY "telemetry"
#define CREARTS_TOPIC_EVENTS "events"
#define CREARTS_TOPIC_ATTRIBUTES "attributes"
#define CREARTS_TOPIC_RPC "rpc"
#define CREARTS_TOPIC_NTP "ntp"
#define CREARTS_TOPIC_OTA "ota"
#define CREARTS_TOPIC_LOGS "logs"

#define CREARTS_TOPIC_DATA "data"
#define CREARTS_TOPIC_POST "post"
#define CREARTS_TOPIC_REPORT "report"
#define CREARTS_TOPIC_REQUEST "request"
#define CREARTS_TOPIC_RESPONSE "response"
#define CREARTS_TOPIC_UPDATE "update"
#define CREARTS_TOPIC_VERSION "version"
#define CREARTS_TOPIC_QUERY "query"
#define CREARTS_TOPIC_CANCEL "cancel"
#define CREARTS_TOPIC_PROGRESS "progress"

#define CREARTS_TOPIC_PATH2(a, b) "/" a "/" b
#define CREARTS_TOPIC_PATH3(a, b, c) "/" a "/" b "/" c

namespace crearts::iot::topic_str {

inline constexpr std::string_view kProtocolRoot = "iot/v1";
inline constexpr std::string_view kShortRoot = "v1";
inline constexpr std::string_view kWildcard = "#";

namespace dir {
inline constexpr std::string_view kUp = CREARTS_TOPIC_UP;
inline constexpr std::string_view kDown = CREARTS_TOPIC_DOWN;
} // namespace dir

namespace cap {
inline constexpr std::string_view kStatus = CREARTS_TOPIC_STATUS;
inline constexpr std::string_view kTelemetry = CREARTS_TOPIC_TELEMETRY;
inline constexpr std::string_view kEvents = CREARTS_TOPIC_EVENTS;
inline constexpr std::string_view kAttributes = CREARTS_TOPIC_ATTRIBUTES;
inline constexpr std::string_view kRpc = CREARTS_TOPIC_RPC;
inline constexpr std::string_view kNtp = CREARTS_TOPIC_NTP;
inline constexpr std::string_view kOta = CREARTS_TOPIC_OTA;
inline constexpr std::string_view kLogs = CREARTS_TOPIC_LOGS;
} // namespace cap

namespace op {
inline constexpr std::string_view kData = CREARTS_TOPIC_DATA;
inline constexpr std::string_view kPost = CREARTS_TOPIC_POST;
inline constexpr std::string_view kReport = CREARTS_TOPIC_REPORT;
inline constexpr std::string_view kRequest = CREARTS_TOPIC_REQUEST;
inline constexpr std::string_view kResponse = CREARTS_TOPIC_RESPONSE;
inline constexpr std::string_view kUpdate = CREARTS_TOPIC_UPDATE;
inline constexpr std::string_view kVersion = CREARTS_TOPIC_VERSION;
inline constexpr std::string_view kQuery = CREARTS_TOPIC_QUERY;
inline constexpr std::string_view kCancel = CREARTS_TOPIC_CANCEL;
inline constexpr std::string_view kProgress = CREARTS_TOPIC_PROGRESS;
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
    CREARTS_TOPIC_PATH2(CREARTS_TOPIC_UP, CREARTS_TOPIC_STATUS);
inline constexpr std::string_view kTelemetry =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_TELEMETRY, CREARTS_TOPIC_DATA);
inline constexpr std::string_view kEvents =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_EVENTS, CREARTS_TOPIC_POST);
inline constexpr std::string_view kAttributesReport =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_ATTRIBUTES, CREARTS_TOPIC_REPORT);
inline constexpr std::string_view kAttributesRequest =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_ATTRIBUTES, CREARTS_TOPIC_REQUEST);
inline constexpr std::string_view kAttributesResponse =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_ATTRIBUTES, CREARTS_TOPIC_RESPONSE);
inline constexpr std::string_view kAttributesUpdate =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_ATTRIBUTES, CREARTS_TOPIC_UPDATE);
inline constexpr std::string_view kRpcRequest =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_RPC, CREARTS_TOPIC_REQUEST);
inline constexpr std::string_view kRpcResponse =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_RPC, CREARTS_TOPIC_RESPONSE);
inline constexpr std::string_view kRpcClientRequest =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_RPC, CREARTS_TOPIC_REQUEST);
inline constexpr std::string_view kRpcClientResponse =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_RPC, CREARTS_TOPIC_RESPONSE);
inline constexpr std::string_view kNtpRequest =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_NTP, CREARTS_TOPIC_REQUEST);
inline constexpr std::string_view kNtpResponse =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_NTP, CREARTS_TOPIC_RESPONSE);
inline constexpr std::string_view kOtaVersion =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_OTA, CREARTS_TOPIC_VERSION);
inline constexpr std::string_view kOtaQuery =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_OTA, CREARTS_TOPIC_QUERY);
inline constexpr std::string_view kOtaUpdate =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_OTA, CREARTS_TOPIC_UPDATE);
inline constexpr std::string_view kOtaCancel =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_DOWN, CREARTS_TOPIC_OTA, CREARTS_TOPIC_CANCEL);
inline constexpr std::string_view kOtaProgress =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_OTA, CREARTS_TOPIC_PROGRESS);
inline constexpr std::string_view kLogs =
    CREARTS_TOPIC_PATH3(CREARTS_TOPIC_UP, CREARTS_TOPIC_LOGS, CREARTS_TOPIC_REPORT);
inline constexpr std::string_view kUpSegment = "/" CREARTS_TOPIC_UP "/";

} // namespace full_suffix

} // namespace crearts::iot::topic_str

// Convenience aliases at crearts::iot level (same names as before).
namespace crearts::iot {
namespace short_topic = topic_str::short_topic;
namespace full_suffix = topic_str::full_suffix;
} // namespace crearts::iot
