#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include "esp_ota_ops.h"
#include <array>
#include <functional>
#include <string>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/ota-update
 */

namespace alicloud::iot {

struct OtaFirmwareInfo {
    std::string version;
    std::string url;
    std::string md5;
    std::string sign;
    std::string signMethod;
    std::string module;
    int32_t     size   = 0;
    bool        isDiff = false;
};

enum class OtaProgressStep : int {
    DownloadError = -2,
    VerifyError   = -3,
    FlashError    = -4,
    UpdateError   = -1,
};

using OtaFirmwareCallback = std::function<bool(const OtaFirmwareInfo& firmware)>;

class OtaModule : public AlicloudBaseModule {
public:
    explicit OtaModule(embed::MqttService& mqtt,
                       std::string_view    productKey,
                       std::string_view    deviceName,
                       std::string         currentVersion,
                       std::string         moduleName = "");

    ~OtaModule() override;

    void setFirmwareCallback(OtaFirmwareCallback cb);

    bool reportVersion();
    bool queryFirmware();
    void checkRollbackState(std::function<bool()> diagnosticFn);

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    std::string         currentVersion_;
    std::string         moduleName_;
    OtaFirmwareCallback firmwareCb_;

    std::string buildOtaTopic(const std::string& suffix) const;

    void handleFirmwareUpgrade(std::string_view payload);
    void handleFirmwareGetReply(std::string_view payload);
    OtaFirmwareInfo parseFirmwareInfo(const void* dataJsonObject) const;
    void performOtaUpdate(const OtaFirmwareInfo& firmware);
    bool verifyMd5(const std::array<uint8_t, 16>& digest, const std::string& expectedMd5) const;
    void reportProgress(int step, const std::string& description);
};

} // namespace alicloud::iot
