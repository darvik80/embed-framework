#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include <string>
#include <vector>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/device-network-status
 */

namespace alicloud::iot {

struct WifiMetrics {
    int         rssi     = 0;
    int         snr      = 0;
    int         per      = 0;
    std::string errStats;
};

struct NetworkStatusData {
    WifiMetrics wifi;
    int64_t     timestamp = 0;
};

class DeviceNetworkStatusModule : public AlicloudBaseModule {
public:
    explicit DeviceNetworkStatusModule(embed::MqttService& mqtt,
                                        std::string_view    productKey,
                                        std::string_view    deviceName);

    ~DeviceNetworkStatusModule() override = default;

    bool reportCurrentStatus(const NetworkStatusData& status);
    bool reportHistoryStatus(const std::vector<NetworkStatusData>& statuses);

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    void handleDiagPostReply(std::string_view payload);
};

} // namespace alicloud::iot
