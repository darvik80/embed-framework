#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_service.hpp"
#include "cogitor_iot/cogitor_iot_service.hpp"

#include <cstdint>

namespace cogitor::iot {

/// NTP time sync via Cogitor IoT MQTT protocol.
///
/// On each MQTT connect: send NTP request, compute clock offset from server
/// response, adjust system time.  Periodic resync every 10 minutes.
///
/// NTP algorithm (spec §NTP):
///   RTT = deviceRecvTime - deviceSendTime
///   serverTime ≈ serverSendTime + RTT / 2
///   offset  = serverTime - deviceRecvTime
class NtpService : public embed::Service {
public:
    const char* serviceName() const override { return "CogitorNtp"; }

    void start() override;
    void stop() override;

    /// True after first successful sync.
    [[nodiscard]] bool synced() const { return synced_; }

    /// Current offset between device clock and server clock (ms).
    [[nodiscard]] int64_t offsetMs() const { return offsetMs_; }

    /// Epoch-ms corrected for offset.
    [[nodiscard]] int64_t nowMs() const;

private:
    IotService* iot_ = nullptr;
    esp_timer_handle_t resyncTimer_ = nullptr;
    uint32_t pendingRequestId_ = 0;
    int64_t offsetMs_ = 0;
    bool synced_ = false;

    embed::Slot<embed::MqttConnected> connectedSlot_{onConnected, this};
    embed::Slot<NtpResponse> ntpResponseSlot_{onNtpResponse, this};

    static void onConnected(const embed::MqttConnected& msg, void* ctx);
    static void onNtpResponse(const NtpResponse& msg, void* ctx);
    static void resyncCallback(void* arg);

    void sendRequest();
    void applyOffset(int64_t offset);
};

} // namespace cogitor::iot
