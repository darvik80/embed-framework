#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_service.hpp"
#include "crearts_iot/crearts_iot_service.hpp"

namespace crearts::iot {

/// On each MQTT connect: publish **reported** static attrs (dashboard reported
/// form), request **desired**, and log desired pushes from `attributes/update`.
class CreartsDeviceInfo : public embed::Service {
public:
    const char* serviceName() const override { return "CreartsDeviceInfo"; }

    void start() override;
    void stop() override;

private:
    CreartsIotService* iot_ = nullptr;
    embed::MqttService* mqtt_ = nullptr;

    embed::Slot<embed::MqttConnected> connectedSlot_{onConnected, this};
    embed::Slot<AttributeUpdate> attrUpdateSlot_{onAttrUpdate, this};
    embed::Slot<AttributeResponse> attrResponseSlot_{onAttrResponse, this};

    static void onConnected(const embed::MqttConnected& msg, void* ctx);
    static void onAttrUpdate(const AttributeUpdate& upd, void* ctx);
    static void onAttrResponse(const AttributeResponse& res, void* ctx);

    static void publishReported(CreartsIotService* iot);
};

} // namespace crearts::iot
