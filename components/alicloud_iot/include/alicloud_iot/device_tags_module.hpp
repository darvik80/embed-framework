#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/device-tags
 */

namespace alicloud::iot {

struct DeviceTag {
    std::string attrKey;
    std::string attrValue;
};

using TagQueryCallback = std::function<void(
    const std::unordered_map<std::string, std::string>& tags,
    const std::string& message_id)>;

class DeviceTagsModule : public AlicloudBaseModule {
public:
    explicit DeviceTagsModule(embed::MqttService& mqtt,
                               std::string_view    productKey,
                               std::string_view    deviceName);

    ~DeviceTagsModule() override = default;

    void setTagQueryCallback(TagQueryCallback cb) { tagQueryCb_ = std::move(cb); }

    bool submitTags(const std::vector<DeviceTag>& tags);
    bool queryTags(const std::vector<std::string>& attrKeys);
    bool deleteTags(const std::vector<std::string>& attrKeys);

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    TagQueryCallback tagQueryCb_;

    void handleTagUpdateReply(std::string_view payload);
    void handleTagQueryReply(std::string_view payload);
    void handleTagDeleteReply(std::string_view payload);
};

} // namespace alicloud::iot
