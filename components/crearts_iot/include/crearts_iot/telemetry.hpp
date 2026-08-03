#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct cJSON;

namespace crearts::iot {

/// Builds telemetry JSON (flat KV, ts+values, or batch) — same shapes as the MQTT spec.
class TelemetryBuilder {
public:
    TelemetryBuilder();
    ~TelemetryBuilder();

    TelemetryBuilder(TelemetryBuilder&& other) noexcept;
    TelemetryBuilder& operator=(TelemetryBuilder&& other) noexcept;

    TelemetryBuilder(const TelemetryBuilder&) = delete;
    TelemetryBuilder& operator=(const TelemetryBuilder&) = delete;

    TelemetryBuilder& timestampMs(int64_t tsMs);

    TelemetryBuilder& add(const char* key, double value);
    TelemetryBuilder& add(const char* key, int64_t value);
    TelemetryBuilder& add(const char* key, int value) {
        return add(key, static_cast<int64_t>(value));
    }
    TelemetryBuilder& add(const char* key, bool value);
    TelemetryBuilder& add(const char* key, const char* value);
    TelemetryBuilder& add(const char* key, std::string_view value);
    TelemetryBuilder& addRawJson(const char* key, std::string_view json);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] int64_t timestampMs() const { return tsMs_; }
    [[nodiscard]] std::string build() const;
    void clear();

    cJSON* releaseValues();

private:
    cJSON* values_ = nullptr;
    int64_t tsMs_ = 0;
    bool ensureValues();
};

class TelemetryBatch {
public:
    TelemetryBatch() = default;
    ~TelemetryBatch();

    TelemetryBatch(TelemetryBatch&& other) noexcept;
    TelemetryBatch& operator=(TelemetryBatch&& other) noexcept;

    TelemetryBatch(const TelemetryBatch&) = delete;
    TelemetryBatch& operator=(const TelemetryBatch&) = delete;

    bool add(TelemetryBuilder entry);

    [[nodiscard]] bool empty() const { return items_.empty(); }
    [[nodiscard]] size_t size() const { return items_.size(); }
    [[nodiscard]] std::string build() const;
    void clear();

private:
    std::vector<cJSON*> items_;
};

} // namespace crearts::iot
