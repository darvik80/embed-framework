#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include <functional>
#include <string>
#include <vector>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/device-log-reporting
 */

namespace alicloud::iot {

enum class LogLevel { FATAL, ERROR, WARN, INFO, DEBUG };

struct DeviceLogEntry {
    std::string utcTime;
    LogLevel    logLevel   = LogLevel::INFO;
    std::string module;
    std::string code;
    std::string traceContext;
    std::string logContent;
};

struct LogConfig {
    int mode = 0; // 0 = not using SDK, 1 = using SDK
};

using LogConfigCallback = std::function<void(const LogConfig& config)>;

class DeviceLogModule : public AlicloudBaseModule {
public:
    explicit DeviceLogModule(embed::MqttService& mqtt,
                              std::string_view    productKey,
                              std::string_view    deviceName);

    ~DeviceLogModule() override = default;

    void setLogConfigCallback(LogConfigCallback cb) { logConfigCb_ = std::move(cb); }

    bool getLogConfig(const std::string& configScope = "device",
                      const std::string& getType     = "content");

    bool reportLogs(const std::vector<DeviceLogEntry>& logs);

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    LogConfigCallback logConfigCb_;

    void handleLogConfigGetReply(std::string_view payload);
    void handleLogConfigPush(std::string_view payload);
    void handleLogPostReply(std::string_view payload);
    LogConfig   parseLogConfig(std::string_view payload);
    std::string logLevelToString(LogLevel level) const;
};

} // namespace alicloud::iot
