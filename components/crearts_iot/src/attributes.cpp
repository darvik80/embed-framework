#include "crearts_iot/attributes.hpp"

#include "cJSON.h"

#include <cstdlib>
#include <string>
#include <utility>

#if __has_include("esp_log.h")
#include "esp_log.h"
static const char* TAG = "CreartsAttrs";
#define CI_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#else
#define CI_LOGW(...) ((void)0)
#endif

namespace crearts::iot {

namespace {

std::string printOrEmpty(cJSON* node)
{
    if (!node) return "{}";
    char* printed = cJSON_PrintUnformatted(node);
    if (!printed) return "{}";
    std::string out(printed);
    free(printed);
    return out;
}

cJSON* parseObject(std::string_view json)
{
    if (json.empty()) return nullptr;
    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return nullptr;
    }
    return root;
}

void addKeyArray(cJSON* root, const char* name, const std::vector<std::string>& keys)
{
    if (keys.empty()) return;
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return;
    for (const auto& key : keys) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(key.c_str()));
    }
    cJSON_AddItemToObject(root, name, arr);
}

} // namespace

AttributeBuilder::AttributeBuilder()
{
    obj_ = cJSON_CreateObject();
}

AttributeBuilder::~AttributeBuilder()
{
    if (obj_) {
        cJSON_Delete(obj_);
        obj_ = nullptr;
    }
}

AttributeBuilder::AttributeBuilder(AttributeBuilder&& other) noexcept
    : obj_(other.obj_)
{
    other.obj_ = nullptr;
}

AttributeBuilder& AttributeBuilder::operator=(AttributeBuilder&& other) noexcept
{
    if (this != &other) {
        if (obj_) cJSON_Delete(obj_);
        obj_ = other.obj_;
        other.obj_ = nullptr;
    }
    return *this;
}

bool AttributeBuilder::ensureObject()
{
    if (!obj_) obj_ = cJSON_CreateObject();
    return obj_ != nullptr;
}

AttributeBuilder& AttributeBuilder::add(const char* key, double value)
{
    if (!key || !ensureObject()) return *this;
    cJSON_AddNumberToObject(obj_, key, value);
    return *this;
}

AttributeBuilder& AttributeBuilder::add(const char* key, int64_t value)
{
    if (!key || !ensureObject()) return *this;
    cJSON_AddNumberToObject(obj_, key, static_cast<double>(value));
    return *this;
}

AttributeBuilder& AttributeBuilder::add(const char* key, bool value)
{
    if (!key || !ensureObject()) return *this;
    cJSON_AddBoolToObject(obj_, key, value);
    return *this;
}

AttributeBuilder& AttributeBuilder::add(const char* key, const char* value)
{
    if (!key || !value || !ensureObject()) return *this;
    cJSON_AddStringToObject(obj_, key, value);
    return *this;
}

AttributeBuilder& AttributeBuilder::add(const char* key, std::string_view value)
{
    if (!key || !ensureObject()) return *this;
    std::string tmp(value);
    cJSON_AddStringToObject(obj_, key, tmp.c_str());
    return *this;
}

AttributeBuilder& AttributeBuilder::addRawJson(const char* key, std::string_view json)
{
    if (!key || json.empty() || !ensureObject()) return *this;
    cJSON* parsed = cJSON_ParseWithLength(json.data(), json.size());
    if (!parsed) {
        CI_LOGW("addRawJson: parse failed for key=%s", key);
        return *this;
    }
    cJSON_AddItemToObject(obj_, key, parsed);
    return *this;
}

bool AttributeBuilder::empty() const
{
    return !obj_ || cJSON_GetArraySize(obj_) == 0;
}

std::string AttributeBuilder::build() const
{
    if (!obj_) return {};
    char* printed = cJSON_PrintUnformatted(obj_);
    if (!printed) return {};
    std::string out(printed);
    free(printed);
    return out;
}

void AttributeBuilder::clear()
{
    if (obj_) {
        cJSON_Delete(obj_);
        obj_ = nullptr;
    }
    obj_ = cJSON_CreateObject();
}

void AttributeRequestBuilder::appendUnique(std::vector<std::string>& keys, std::string_view key)
{
    while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.remove_prefix(1);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.remove_suffix(1);
    if (key.empty()) return;
    for (const auto& existing : keys) {
        if (existing == key) return;
    }
    keys.emplace_back(key);
}

AttributeRequestBuilder& AttributeRequestBuilder::id(uint32_t requestId)
{
    id_ = requestId;
    hasId_ = true;
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::addReported(std::string_view key)
{
    appendUnique(reported_, key);
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::addDesired(std::string_view key)
{
    appendUnique(desired_, key);
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::reportedAll()
{
    reported_.clear();
    reported_.emplace_back("*");
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::desiredAll()
{
    desired_.clear();
    desired_.emplace_back("*");
    return *this;
}

std::string AttributeRequestBuilder::build() const
{
    cJSON* root = cJSON_CreateObject();
    if (!root) return {};

    if (hasId_) {
        cJSON_AddNumberToObject(root, "id", static_cast<double>(id_));
    }
    addKeyArray(root, "reported", reported_);
    addKeyArray(root, "desired", desired_);

    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return {};
    std::string out(printed);
    free(printed);
    return out;
}

void AttributeRequestBuilder::clear()
{
    id_ = 0;
    hasId_ = false;
    reported_.clear();
    desired_.clear();
}

AttributeValues parseAttributeResponse(std::string_view payload)
{
    AttributeValues out;
    out.reportedJson = "{}";
    out.desiredJson = "{}";

    cJSON* root = parseObject(payload);
    if (!root) {
        CI_LOGW("parseAttributeResponse: invalid JSON");
        return out;
    }

    cJSON* reported = cJSON_GetObjectItemCaseSensitive(root, "reported");
    cJSON* desired = cJSON_GetObjectItemCaseSensitive(root, "desired");
    if (cJSON_IsObject(reported)) out.reportedJson = printOrEmpty(reported);
    if (cJSON_IsObject(desired)) out.desiredJson = printOrEmpty(desired);

    cJSON_Delete(root);
    return out;
}

AttributeValues parseAttributeUpdate(std::string_view payload)
{
    AttributeValues out;
    out.reportedJson = "{}";
    out.desiredJson = "{}";

    cJSON* root = parseObject(payload);
    if (!root) {
        CI_LOGW("parseAttributeUpdate: invalid JSON");
        return out;
    }

    out.desiredJson = printOrEmpty(root);
    cJSON_Delete(root);
    return out;
}

bool attributeGetNumber(std::string_view objectJson, const char* key, double& out)
{
    if (!key) return false;
    cJSON* root = parseObject(objectJson);
    if (!root) return false;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    bool ok = cJSON_IsNumber(item);
    if (ok) out = item->valuedouble;
    cJSON_Delete(root);
    return ok;
}

bool attributeGetBool(std::string_view objectJson, const char* key, bool& out)
{
    if (!key) return false;
    cJSON* root = parseObject(objectJson);
    if (!root) return false;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    bool ok = cJSON_IsBool(item);
    if (ok) out = cJSON_IsTrue(item);
    cJSON_Delete(root);
    return ok;
}

bool attributeGetString(std::string_view objectJson, const char* key, std::string& out)
{
    if (!key) return false;
    cJSON* root = parseObject(objectJson);
    if (!root) return false;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    bool ok = cJSON_IsString(item) && item->valuestring;
    if (ok) out = item->valuestring;
    cJSON_Delete(root);
    return ok;
}

uint32_t parseJsonId(std::string_view payload)
{
    cJSON* root = parseObject(payload);
    if (!root) return 0;
    cJSON* id = cJSON_GetObjectItemCaseSensitive(root, "id");
    uint32_t out = 0;
    if (cJSON_IsNumber(id) && id->valuedouble > 0) {
        out = static_cast<uint32_t>(id->valuedouble);
    }
    cJSON_Delete(root);
    return out;
}

} // namespace crearts::iot
