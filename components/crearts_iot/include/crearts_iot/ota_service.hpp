#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_service.hpp"
#include "crearts_iot/crearts_iot_service.hpp"
#include "esp_timer.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace crearts::iot {

/// Applies Crearts OTA from `v1/me/o/upd` / `…/down/ota/update`.
/// Download + flash run on a dedicated FreeRTOS task (not the embed EventLoop).
/// Requires dual OTA partitions (`partitions_ota.csv`).
class OtaService : public embed::Service {
public:
    const char* serviceName() const override { return "CreartsOtaService"; }

    void start() override;
    void stop() override;

    [[nodiscard]] bool inProgress() const { return inProgress_.load(); }

    struct Firmware {
        std::string version;
        std::string module = "main";
        std::string url;
        std::string sha256;
        std::string stream;
        int32_t size = 0;
        bool force = false;
    };

private:
    IotService* iot_ = nullptr;
    esp_timer_handle_t verifyTimer_ = nullptr;
    std::atomic<bool> inProgress_{false};
    std::atomic<bool> cancel_{false};

    embed::Slot<OtaUpdate> updateSlot_{onUpdate, this};
    embed::Slot<OtaCancel> cancelSlot_{onCancel, this};
    embed::Slot<embed::MqttConnected> mqttConnectedSlot_{onMqttConnected, this};

    static void onUpdate(const OtaUpdate& msg, void* ctx);
    static void onCancel(const OtaCancel& msg, void* ctx);
    static void onMqttConnected(const embed::MqttConnected& msg, void* ctx);
    static void verifyTimeout(void* arg);
    static void otaTask(void* arg);

    void handleUpdate(std::string_view payload);
    void handleCancel(std::string_view payload);
    void schedule(Firmware fw);
    void perform(const Firmware& fw);
    void report(const char* module, int step, const char* desc);
    void confirmPendingImage();

    [[nodiscard]] static bool parseFirmware(std::string_view json, Firmware& out);
    [[nodiscard]] static bool hexEqual(const uint8_t* digest, size_t digestLen, std::string_view hex);
};

} // namespace crearts::iot
