#include "crearts_iot/ntp_service.hpp"

#include "embed/registry.hpp"
#include "esp_log.h"
#include "esp_timer.h"

#include <sys/time.h>
#include <ctime>

namespace crearts::iot {

static const char* TAG = "Ntp";
static constexpr int64_t kResyncIntervalUs = 10 * 60 * 1000 * 1000LL; // 10 min

static int64_t epochMs()
{
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

void NtpService::start()
{
    auto& reg = embed::ServiceRegistry::instance();
    iot_ = reg.getService<IotService>();
    auto* mqtt = reg.getService<embed::MqttService>();
    if (!iot_ || !mqtt) {
        ESP_LOGE(TAG, "IotService/MqttService missing");
        return;
    }

    connectedSlot_.connect(mqtt->onConnected);
    ntpResponseSlot_.connect(iot_->onNtpResponse);

    const esp_timer_create_args_t args = {
        .callback = resyncCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ntp_resync",
        .skip_unhandled_events = true
    };
    esp_timer_create(&args, &resyncTimer_);

    ESP_LOGI(TAG, "Started — will sync time on MQTT connect");
}

void NtpService::stop()
{
    if (resyncTimer_) {
        esp_timer_stop(resyncTimer_);
        esp_timer_delete(resyncTimer_);
        resyncTimer_ = nullptr;
    }
    connectedSlot_.disconnect();
    ntpResponseSlot_.disconnect();
    iot_ = nullptr;
    synced_ = false;
}

int64_t NtpService::nowMs() const
{
    return epochMs() + offsetMs_;
}

void NtpService::onConnected(const embed::MqttConnected&, void* ctx)
{
    auto* self = static_cast<NtpService*>(ctx);
    if (!self->iot_) return;
    self->sendRequest();
}

void NtpService::onNtpResponse(const NtpResponse& res, void* ctx)
{
    auto* self = static_cast<NtpService*>(ctx);
    if (!self->iot_) return;

    if (res.requestId != self->pendingRequestId_) {
        ESP_LOGW(TAG, "NTP response id=%lu != pending=%lu — ignoring",
                 static_cast<unsigned long>(res.requestId),
                 static_cast<unsigned long>(self->pendingRequestId_));
        return;
    }

    const int64_t deviceRecvTime = epochMs();
    const int64_t rtt = deviceRecvTime - res.deviceSendTime;
    if (rtt < 0 || rtt > 30000) {
        ESP_LOGW(TAG, "NTP RTT out of range: %lld ms", static_cast<long long>(rtt));
        return;
    }

    // serverTime ≈ serverSendTime + RTT/2
    const int64_t serverTime = res.serverSendTime + rtt / 2;
    const int64_t offset = serverTime - deviceRecvTime;

    ESP_LOGI(TAG, "NTP sync: rtt=%lld offset=%lld serverTime=%lld",
             static_cast<long long>(rtt),
             static_cast<long long>(offset),
             static_cast<long long>(serverTime));

    self->applyOffset(offset);
}

void NtpService::resyncCallback(void* arg)
{
    auto* self = static_cast<NtpService*>(arg);
    if (self->iot_) {
        ESP_LOGD(TAG, "Periodic NTP resync");
        self->sendRequest();
    }
}

void NtpService::sendRequest()
{
    if (!iot_) return;
    const int64_t sendTime = epochMs();
    const int msgId = iot_->requestNtp(pendingRequestId_, sendTime, 1);
    if (msgId < 0) {
        ESP_LOGW(TAG, "Failed to send NTP request");
        return;
    }
    ESP_LOGI(TAG, "NTP request sent id=%lu msg_id=%d",
             static_cast<unsigned long>(pendingRequestId_), msgId);

    // Schedule next resync
    if (resyncTimer_) {
        esp_timer_stop(resyncTimer_);
        esp_timer_start_once(resyncTimer_, kResyncIntervalUs);
    }
}

void NtpService::applyOffset(int64_t offset)
{
    offsetMs_ = offset;
    synced_ = true;

    // Apply to system clock if offset is significant (> 100 ms)
    if (offset > 100 || offset < -100) {
        struct timeval tv{};
        gettimeofday(&tv, nullptr);
        const int64_t correctedUs = (static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec) + offset * 1000;
        tv.tv_sec = static_cast<time_t>(correctedUs / 1000000);
        tv.tv_usec = static_cast<suseconds_t>((correctedUs % 1000000 + 1000000) % 1000000);
        if (settimeofday(&tv, nullptr) == 0) {
            ESP_LOGI(TAG, "System clock adjusted by %lld ms", static_cast<long long>(offset));
        } else {
            ESP_LOGW(TAG, "settimeofday failed");
        }
        // After applying, offset is absorbed into system clock
        offsetMs_ = 0;
    }
}

} // namespace crearts::iot
