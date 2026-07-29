#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct cJSON;

namespace thingsboard {

/// Builds ThingsBoard telemetry JSON payloads.
///
/// Formats (https://thingsboard.io/docs/reference/mqtt-api/telemetry/):
///   1. Simple KV — server timestamp:
///        {"temperature":22.5,"humidity":61}
///   2. Client timestamp:
///        {"ts":1451649600512,"values":{"temperature":22.5}}
///   3. Batch — via TelemetryBatch (array of ts+values objects)
///
/// Not copyable (owns cJSON). Move-only.
class TelemetryBuilder {
public:
    TelemetryBuilder();
    ~TelemetryBuilder();

    TelemetryBuilder(TelemetryBuilder&& other) noexcept;
    TelemetryBuilder& operator=(TelemetryBuilder&& other) noexcept;

    TelemetryBuilder(const TelemetryBuilder&) = delete;
    TelemetryBuilder& operator=(const TelemetryBuilder&) = delete;

    /// When set (non-zero), build() emits {"ts":...,"values":{...}}.
    /// When 0 (default), build() emits the flat key-value object.
    TelemetryBuilder& timestampMs(int64_t tsMs);

    TelemetryBuilder& add(const char* key, double value);
    TelemetryBuilder& add(const char* key, int64_t value);
    TelemetryBuilder& add(const char* key, int value) { return add(key, static_cast<int64_t>(value)); }
    TelemetryBuilder& add(const char* key, bool value);
    TelemetryBuilder& add(const char* key, const char* value);
    TelemetryBuilder& add(const char* key, std::string_view value);

    /// Parse `json` and attach as nested object/array under key (or skip on parse error).
    TelemetryBuilder& addRawJson(const char* key, std::string_view json);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] int64_t timestampMs() const { return tsMs_; }

    /// Serialize according to timestampMs(). Empty string on failure.
    [[nodiscard]] std::string build() const;

    void clear();

    /// Steal values object for batch assembly (leaves builder empty).
    cJSON* releaseValues();

private:
    cJSON* values_ = nullptr;
    int64_t tsMs_ = 0;

    bool ensureValues();
};

/// Builds a JSON array of {"ts":...,"values":{...}} entries.
class TelemetryBatch {
public:
    TelemetryBatch() = default;
    ~TelemetryBatch();

    TelemetryBatch(TelemetryBatch&& other) noexcept;
    TelemetryBatch& operator=(TelemetryBatch&& other) noexcept;

    TelemetryBatch(const TelemetryBatch&) = delete;
    TelemetryBatch& operator=(const TelemetryBatch&) = delete;

    /// Append one reading. `entry.timestampMs()` must be non-zero.
    /// Returns false if ts missing or entry empty.
    bool add(TelemetryBuilder entry);

    [[nodiscard]] bool empty() const { return items_.empty(); }
    [[nodiscard]] size_t size() const { return items_.size(); }

    [[nodiscard]] std::string build() const;
    void clear();

private:
    std::vector<cJSON*> items_;
};

} // namespace thingsboard
