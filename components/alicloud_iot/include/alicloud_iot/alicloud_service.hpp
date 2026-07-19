#pragma once

#include "embed/service.hpp"
#include "embed/slot.hpp"
#include "embed_core/mqtt_service.hpp"
#include "alicloud_iot/alicloud_module.hpp"
#include "alicloud_iot/things_module.hpp"
#include "alicloud_iot/ota_module.hpp"
#include "alicloud_iot/ntp_module.hpp"
#include "alicloud_iot/remote_config_module.hpp"
#include "alicloud_iot/device_tags_module.hpp"
#include "alicloud_iot/device_log_module.hpp"
#include "alicloud_iot/device_network_status_module.hpp"
#include <memory>
#include <string_view>

namespace alicloud::iot {

/**
 * @brief Top-level embed::Service that owns all Alibaba Cloud IoT Alink modules.
 *
 * Architecture (hybrid pool/heap):
 *   - AlicloudService itself lives in the ServiceRegistry pool (~160 bytes)
 *   - All 7 modules are heap-allocated via std::unique_ptr
 *   - Three Slot<> members connect to MqttService signals
 *
 * Lifecycle:
 *   1. Registry creates AlicloudService (modules are NOT yet created)
 *   2. start() — retrieves MqttService from registry, creates all modules,
 *      connects slots to MqttService signals
 *   3. On MQTT connected → notifies all modules
 *   4. On MQTT message  → dispatches to all modules
 *   5. On MQTT disconnected → notifies all modules
 */
class AlicloudService : public embed::Service {
public:
    const char* serviceName() const override { return "AlicloudService"; }

    void start() override;
    void stop()  override;

    // Accessors for application-level use
    ThingsModule*             things()        const { return things_.get();        }
    OtaModule*                ota()           const { return ota_.get();           }
    NtpModule*                ntp()           const { return ntp_.get();           }
    RemoteConfigModule*       remoteConfig()  const { return remoteConfig_.get();  }
    DeviceTagsModule*         deviceTags()    const { return deviceTags_.get();    }
    DeviceLogModule*          deviceLog()     const { return deviceLog_.get();     }
    DeviceNetworkStatusModule* networkStatus() const { return networkStatus_.get(); }

private:
    // Set in start() — non-owning pointer to MqttService in registry
    embed::MqttService* mqtt_       = nullptr;
    std::string_view    productKey_ = CONFIG_EMBED_ALICLOUD_PRODUCT_KEY;
    std::string_view    deviceName_ = CONFIG_EMBED_ALICLOUD_DEVICE_NAME;

    // Heap-allocated modules (created in start())
    std::unique_ptr<ThingsModule>             things_;
    std::unique_ptr<OtaModule>                ota_;
    std::unique_ptr<NtpModule>                ntp_;
    std::unique_ptr<RemoteConfigModule>       remoteConfig_;
    std::unique_ptr<DeviceTagsModule>         deviceTags_;
    std::unique_ptr<DeviceLogModule>          deviceLog_;
    std::unique_ptr<DeviceNetworkStatusModule> networkStatus_;

    // Slots connected to MqttService signals (connected in start())
    embed::Slot<embed::MqttConnected>        mqttConnectedSlot_{onMqttConnected, this};
    embed::Slot<embed::MqttDisconnected>     mqttDisconnectedSlot_{onMqttDisconnected, this};
    embed::Slot<embed::MqttMessageReceived>  mqttMessageSlot_{onMqttMessage, this};

    static void onMqttConnected   (const embed::MqttConnected&        msg, void* ctx);
    static void onMqttDisconnected(const embed::MqttDisconnected&      msg, void* ctx);
    static void onMqttMessage     (const embed::MqttMessageReceived&   msg, void* ctx);

    /// Dispatch incoming MQTT message to every module.
    void dispatchMessage(std::string_view topic, const char* data, int len);

    /// Notify all modules that MQTT is connected.
    void notifyConnected();

    /// Notify all modules that MQTT is disconnected.
    void notifyDisconnected();
};

} // namespace alicloud::iot
