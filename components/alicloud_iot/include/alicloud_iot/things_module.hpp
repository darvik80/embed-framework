#pragma once

#include "alicloud_iot/alicloud_module.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/**
 * Ref: https://www.alibabacloud.com/help/en/iot/user-guide/device-properties-events-and-services
 */

namespace alicloud::iot {

using VariantValue = std::variant<int, double, bool, std::string>;
using Params       = std::unordered_map<std::string, VariantValue>;

struct PropertyData {
    std::variant<std::string, int, double, bool> value;
    int64_t time = 0;
};

struct EventData {
    std::unordered_map<std::string, std::string> params;
    int64_t time = 0;
};

struct ServiceResponse {
    int         code    = 0;
    std::string message;
    std::unordered_map<std::string, VariantValue> data;
    std::string id;
    std::string version = "1.0";
};

using PropertySetCallback    = std::function<bool(
    const std::unordered_map<std::string, std::string>& properties,
    const std::string& message_id)>;

using ServiceInvokeCallback  = std::function<ServiceResponse(
    const std::unordered_map<std::string, VariantValue>& params,
    const std::string& message_id)>;

using DesiredPropertyCallback = std::function<void(
    const std::unordered_map<std::string, PropertyData>& properties)>;

class ThingsModule : public AlicloudBaseModule {
public:
    explicit ThingsModule(embed::MqttService& mqtt,
                          std::string_view productKey,
                          std::string_view deviceName);
    ~ThingsModule() override = default;

    void setPropertySetCallback(PropertySetCallback cb)  { propertySetCb_ = std::move(cb); }
    void setDesiredPropertyCallback(DesiredPropertyCallback cb) { desiredCb_ = std::move(cb); }
    void addServiceInvokeCallback(const std::string& method, ServiceInvokeCallback cb);
    void removeServiceInvokeCallback(const std::string& method);

    bool reportProperties(const std::unordered_map<std::string, PropertyData>& properties);
    bool postEvent(const std::string& event_id, const EventData& event_data);
    bool getDesiredProperties(const std::vector<std::string>& property_ids);
    bool deleteDesiredProperties(const std::unordered_map<std::string, std::optional<int>>& properties);
    void sendPropertySetResponse(const std::string& message_id, int code, const std::string& message = "success");
    void sendServiceResponse(const std::string& message_id, const ServiceResponse& response);

    void handleMqttData(std::string_view topic, const char* data, int data_len) override;

protected:
    bool subscribeTopics()   override;
    bool unsubscribeTopics() override;

private:
    PropertySetCallback                                       propertySetCb_;
    std::unordered_map<std::string, ServiceInvokeCallback>   serviceInvokeCbs_;
    DesiredPropertyCallback                                   desiredCb_;

    void handlePropertySet(std::string_view payload);
    void handleServiceInvoke(std::string_view topic, std::string_view payload);
    void handleDesiredPropertyGet(std::string_view payload);
    void handleDesiredPropertyDelete(std::string_view payload);
    ServiceResponse parseResponse(std::string_view payload);
    std::string buildRequest(const std::string& method, const std::string& params, const std::string& message_id);
};

} // namespace alicloud::iot
