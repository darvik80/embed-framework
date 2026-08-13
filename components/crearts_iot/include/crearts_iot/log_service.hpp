#pragma once

#include "embed/embed.hpp"
#include "embed_core/mqtt_service.hpp"
#include "crearts_iot/crearts_iot_service.hpp"

#include <cstdint>

namespace crearts::iot {

/// Forwards ESP-IDF log output to Crearts IoT `logs/report` topic.
///
/// Installs a custom vprintf via `esp_log_set_vprintf`.  Every log line is
/// formatted as a JSON object per the IoT spec (§Device Logs) and published
/// with QoS 0.  Original console output is preserved.
///
/// Re-entrancy guard: logs generated inside publishLogs are dropped.
/// A flush timer batches buffered entries every 2 seconds.
class LogService : public embed::Service {
public:
    const char* serviceName() const override { return "CreartsLog"; }

    void start() override;
    void stop() override;

    /// Set minimum level to forward (0=ERROR … 4=VERBOSE, -1=disable).
    void setMinLevel(int level) { minLevel_ = level; }

private:
    IotService* iot_ = nullptr;
    int minLevel_ = 1; // WARN and above by default

    // Original vprintf — restored on stop.
    vprintf_like_t originalVprintf_ = nullptr;

    // Batch buffer: accumulate JSON objects, flush as array.
    static constexpr int kBufSize = 1536;
    char batchBuf_[kBufSize]{};
    int batchLen_ = 0;
    int entryCount_ = 0;

    // Re-entrancy guard.
    static thread_local bool publishing_;

    esp_timer_handle_t flushTimer_ = nullptr;

    static constexpr int64_t kFlushIntervalUs = 2 * 1000 * 1000LL; // 2 s

    static int logVprintf(const char* fmt, va_list ap);
    static void flushCallback(void* arg);

    void appendEntry(int level, const char* tag, const char* msg);
    void flush();

    static const char* levelName(int level);
};

} // namespace crearts::iot
