#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include <functional>
#include <string>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/ntp-service
 *
 * NTP topics use /ext/ntp/{productKey}/{deviceName}/{request|response}
 * (different from the /sys/ prefix used by most Alink topics)
 */

namespace alicloud::iot {

using NtpSyncCallback = std::function<void(int64_t serverTime,
                                            int64_t deviceSendTime,
                                            int64_t serverRecvTime,
                                            int64_t serverSendTime)>;

class NtpModule : public AlicloudBaseModule {
public:
    explicit NtpModule(embed::MqttService& mqtt,
                       std::string_view    productKey,
                       std::string_view    deviceName);

    ~NtpModule() override = default;

    void setNtpSyncCallback(NtpSyncCallback cb) { ntpCb_ = std::move(cb); }

    bool requestTimeSync();

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    NtpSyncCallback ntpCb_;
    int64_t         deviceSendTime_ = 0;

    /// Build /ext/ntp/{product}/{device}/{suffix}
    std::string buildNtpTopic(std::string_view suffix) const;

    void handleNtpResponse(std::string_view payload);

    static bool    setSystemTime(int64_t serverTimeMs);
};

} // namespace alicloud::iot
