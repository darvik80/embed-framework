#include "thingsboard/telemetry.hpp"

#include "cJSON.h"

#include <cstdlib>
#include <string>
#include <utility>

#if __has_include("esp_log.h")
#include "esp_log.h"
static const char* TAG = "TbTelemetry";
#define TB_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#else
#define TB_LOGW(...) ((void)0)
#endif

namespace thingsboard {

TelemetryBuilder::TelemetryBuilder()
{
    values_ = cJSON_CreateObject();
}

TelemetryBuilder::~TelemetryBuilder()
{
    if (values_) {
        cJSON_Delete(values_);
        values_ = nullptr;
    }
}

TelemetryBuilder::TelemetryBuilder(TelemetryBuilder&& other) noexcept
    : values_(other.values_)
    , tsMs_(other.tsMs_)
{
    other.values_ = nullptr;
    other.tsMs_ = 0;
}

TelemetryBuilder& TelemetryBuilder::operator=(TelemetryBuilder&& other) noexcept
{
    if (this != &other) {
        if (values_) cJSON_Delete(values_);
        values_ = other.values_;
        tsMs_ = other.tsMs_;
        other.values_ = nullptr;
        other.tsMs_ = 0;
    }
    return *this;
}

bool TelemetryBuilder::ensureValues()
{
    if (!values_) {
        values_ = cJSON_CreateObject();
    }
    return values_ != nullptr;
}

TelemetryBuilder& TelemetryBuilder::timestampMs(int64_t tsMs)
{
    tsMs_ = tsMs;
    return *this;
}

TelemetryBuilder& TelemetryBuilder::add(const char* key, double value)
{
    if (!key || !ensureValues()) return *this;
    cJSON_AddNumberToObject(values_, key, value);
    return *this;
}

TelemetryBuilder& TelemetryBuilder::add(const char* key, int64_t value)
{
    if (!key || !ensureValues()) return *this;
    cJSON_AddNumberToObject(values_, key, static_cast<double>(value));
    return *this;
}

TelemetryBuilder& TelemetryBuilder::add(const char* key, bool value)
{
    if (!key || !ensureValues()) return *this;
    cJSON_AddBoolToObject(values_, key, value);
    return *this;
}

TelemetryBuilder& TelemetryBuilder::add(const char* key, const char* value)
{
    if (!key || !value || !ensureValues()) return *this;
    cJSON_AddStringToObject(values_, key, value);
    return *this;
}

TelemetryBuilder& TelemetryBuilder::add(const char* key, std::string_view value)
{
    if (!key || !ensureValues()) return *this;
    std::string tmp(value);
    cJSON_AddStringToObject(values_, key, tmp.c_str());
    return *this;
}

TelemetryBuilder& TelemetryBuilder::addRawJson(const char* key, std::string_view json)
{
    if (!key || json.empty() || !ensureValues()) return *this;
    cJSON* parsed = cJSON_ParseWithLength(json.data(), json.size());
    if (!parsed) {
        TB_LOGW("addRawJson: parse failed for key=%s", key);
        return *this;
    }
    cJSON_AddItemToObject(values_, key, parsed);
    return *this;
}

bool TelemetryBuilder::empty() const
{
    return !values_ || cJSON_GetArraySize(values_) == 0;
}

std::string TelemetryBuilder::build() const
{
    if (!values_) return {};

    cJSON* root = nullptr;
    if (tsMs_ == 0) {
        root = cJSON_Duplicate(values_, true);
    } else {
        root = cJSON_CreateObject();
        if (!root) return {};
        cJSON_AddNumberToObject(root, "ts", static_cast<double>(tsMs_));
        cJSON* valuesCopy = cJSON_Duplicate(values_, true);
        if (!valuesCopy) {
            cJSON_Delete(root);
            return {};
        }
        cJSON_AddItemToObject(root, "values", valuesCopy);
    }

    if (!root) return {};
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return {};
    std::string out(printed);
    free(printed);
    return out;
}

void TelemetryBuilder::clear()
{
    if (values_) {
        cJSON_Delete(values_);
        values_ = nullptr;
    }
    tsMs_ = 0;
    values_ = cJSON_CreateObject();
}

cJSON* TelemetryBuilder::releaseValues()
{
    cJSON* out = values_;
    values_ = nullptr;
    tsMs_ = 0;
    return out;
}

TelemetryBatch::~TelemetryBatch()
{
    clear();
}

TelemetryBatch::TelemetryBatch(TelemetryBatch&& other) noexcept
    : items_(std::move(other.items_))
{}

TelemetryBatch& TelemetryBatch::operator=(TelemetryBatch&& other) noexcept
{
    if (this != &other) {
        clear();
        items_ = std::move(other.items_);
    }
    return *this;
}

bool TelemetryBatch::add(TelemetryBuilder entry)
{
    if (entry.timestampMs() == 0) {
        TB_LOGW("TelemetryBatch entry requires timestampMs()");
        return false;
    }
    if (entry.empty()) {
        TB_LOGW("TelemetryBatch entry is empty");
        return false;
    }

    cJSON* obj = cJSON_CreateObject();
    if (!obj) return false;
    cJSON_AddNumberToObject(obj, "ts", static_cast<double>(entry.timestampMs()));
    cJSON* values = entry.releaseValues();
    if (!values) {
        cJSON_Delete(obj);
        return false;
    }
    cJSON_AddItemToObject(obj, "values", values);
    items_.push_back(obj);
    return true;
}

std::string TelemetryBatch::build() const
{
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return {};

    for (cJSON* item : items_) {
        cJSON* copy = cJSON_Duplicate(item, true);
        if (copy) cJSON_AddItemToArray(arr, copy);
    }

    char* printed = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!printed) return {};
    std::string out(printed);
    free(printed);
    return out;
}

void TelemetryBatch::clear()
{
    for (cJSON* item : items_) {
        cJSON_Delete(item);
    }
    items_.clear();
}

} // namespace thingsboard
