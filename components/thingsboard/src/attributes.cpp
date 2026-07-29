#include "thingsboard/attributes.hpp"

#include "cJSON.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#if __has_include("esp_log.h")
#include "esp_log.h"
static const char* TAG = "TbAttributes";
#define TB_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#else
#define TB_LOGW(...) ((void)0)
#endif

namespace thingsboard {

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

} // namespace

// ── AttributeBuilder ────────────────────────────────────────────────────

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
        TB_LOGW("addRawJson: parse failed for key=%s", key);
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

// ── AttributeRequestBuilder ─────────────────────────────────────────────

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

void AttributeRequestBuilder::appendCsv(std::vector<std::string>& keys, std::string_view csv)
{
    size_t start = 0;
    while (start <= csv.size()) {
        size_t comma = csv.find(',', start);
        if (comma == std::string_view::npos) {
            appendUnique(keys, csv.substr(start));
            break;
        }
        appendUnique(keys, csv.substr(start, comma - start));
        start = comma + 1;
    }
}

std::string AttributeRequestBuilder::joinCsv(const std::vector<std::string>& keys)
{
    std::string out;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) out.push_back(',');
        out += keys[i];
    }
    return out;
}

AttributeRequestBuilder& AttributeRequestBuilder::clientKeys(std::string_view commaSeparated)
{
    appendCsv(clientKeys_, commaSeparated);
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::sharedKeys(std::string_view commaSeparated)
{
    appendCsv(sharedKeys_, commaSeparated);
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::addClientKey(std::string_view key)
{
    appendUnique(clientKeys_, key);
    return *this;
}

AttributeRequestBuilder& AttributeRequestBuilder::addSharedKey(std::string_view key)
{
    appendUnique(sharedKeys_, key);
    return *this;
}

std::string AttributeRequestBuilder::build() const
{
    cJSON* root = cJSON_CreateObject();
    if (!root) return {};

    if (!clientKeys_.empty()) {
        const std::string csv = joinCsv(clientKeys_);
        cJSON_AddStringToObject(root, "clientKeys", csv.c_str());
    }
    if (!sharedKeys_.empty()) {
        const std::string csv = joinCsv(sharedKeys_);
        cJSON_AddStringToObject(root, "sharedKeys", csv.c_str());
    }

    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) return {};
    std::string out(printed);
    free(printed);
    return out;
}

void AttributeRequestBuilder::clear()
{
    clientKeys_.clear();
    sharedKeys_.clear();
}

// ── Parsers ─────────────────────────────────────────────────────────────

AttributeValues parseAttributeResponse(std::string_view payload)
{
    AttributeValues out;
    out.clientJson = "{}";
    out.sharedJson = "{}";

    cJSON* root = parseObject(payload);
    if (!root) {
        TB_LOGW("parseAttributeResponse: invalid JSON");
        return out;
    }

    cJSON* client = cJSON_GetObjectItemCaseSensitive(root, "client");
    cJSON* shared = cJSON_GetObjectItemCaseSensitive(root, "shared");
    if (cJSON_IsObject(client)) out.clientJson = printOrEmpty(client);
    if (cJSON_IsObject(shared)) out.sharedJson = printOrEmpty(shared);

    cJSON_Delete(root);
    return out;
}

AttributeValues parseAttributeUpdate(std::string_view payload)
{
    AttributeValues out;
    out.clientJson = "{}";
    out.sharedJson = "{}";

    cJSON* root = parseObject(payload);
    if (!root) {
        TB_LOGW("parseAttributeUpdate: invalid JSON");
        return out;
    }

    // Pushed shared updates are a flat object of changed keys.
    out.sharedJson = printOrEmpty(root);
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

} // namespace thingsboard
