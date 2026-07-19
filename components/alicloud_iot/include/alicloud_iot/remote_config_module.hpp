#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include <functional>
#include <string>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/remote-configuration-1
 */

namespace alicloud::iot {

struct RemoteConfigData {
    std::string configId;
    int64_t     configSize  = 0;
    std::string sign;
    std::string signMethod;
    std::string url;
    std::string getType;
};

using RemoteConfigCallback = std::function<void(const RemoteConfigData& config,
                                                 const std::string& message_id)>;

class RemoteConfigModule : public AlicloudBaseModule {
public:
    explicit RemoteConfigModule(embed::MqttService& mqtt,
                                 std::string_view    productKey,
                                 std::string_view    deviceName);

    ~RemoteConfigModule() override = default;

    void setRemoteConfigCallback(RemoteConfigCallback cb) { configCb_ = std::move(cb); }

    bool getConfig(const std::string& configScope = "product",
                   const std::string& getType     = "file");

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    RemoteConfigCallback configCb_;

    void handleConfigGetReply(std::string_view payload);
    void handleConfigPush(std::string_view payload);
    RemoteConfigData parseConfigData(std::string_view payload);
};

} // namespace alicloud::iot
