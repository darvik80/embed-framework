#include "thingsboard/rpc.hpp"

#include "cJSON.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#if __has_include("esp_log.h")
#include "esp_log.h"
static const char* TAG = "TbRpc";
#define TB_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#else
#define TB_LOGW(...) ((void)0)
#endif

namespace thingsboard {

namespace {

std::string printOrEmpty(cJSON* node)
{
    if (!node) return {};
    char* printed = cJSON_PrintUnformatted(node);
    if (!printed) return {};
    std::string out(printed);
    free(printed);
    return out;
}

} // namespace

// ── RpcBuilder ──────────────────────────────────────────────────────────

RpcBuilder::RpcBuilder()
{
    root_ = cJSON_CreateObject();
}

RpcBuilder::~RpcBuilder()
{
    if (root_) {
        cJSON_Delete(root_);
        root_ = nullptr;
    }
}

RpcBuilder::RpcBuilder(RpcBuilder&& other) noexcept
    : root_(other.root_)
{
    other.root_ = nullptr;
}

RpcBuilder& RpcBuilder::operator=(RpcBuilder&& other) noexcept
{
    if (this != &other) {
        if (root_) cJSON_Delete(root_);
        root_ = other.root_;
        other.root_ = nullptr;
    }
    return *this;
}

bool RpcBuilder::ensureRoot()
{
    if (!root_) root_ = cJSON_CreateObject();
    return root_ != nullptr;
}

cJSON* RpcBuilder::ensureParamsObject()
{
    if (!ensureRoot()) return nullptr;
    cJSON* params = cJSON_GetObjectItemCaseSensitive(root_, "params");
    if (params && cJSON_IsObject(params)) {
        return params;
    }
    if (params) {
        cJSON_DeleteItemFromObject(root_, "params");
    }
    params = cJSON_CreateObject();
    if (!params) return nullptr;
    cJSON_AddItemToObject(root_, "params", params);
    return params;
}

RpcBuilder& RpcBuilder::method(const char* name)
{
    if (!name || !ensureRoot()) return *this;
    cJSON_DeleteItemFromObject(root_, "method");
    cJSON_AddStringToObject(root_, "method", name);
    return *this;
}

RpcBuilder& RpcBuilder::method(std::string_view name)
{
    if (name.empty() || !ensureRoot()) return *this;
    std::string tmp(name);
    return method(tmp.c_str());
}

RpcBuilder& RpcBuilder::param(const char* key, double value)
{
    if (!key) return *this;
    cJSON* params = ensureParamsObject();
    if (!params) return *this;
    cJSON_AddNumberToObject(params, key, value);
    return *this;
}

RpcBuilder& RpcBuilder::param(const char* key, int64_t value)
{
    return param(key, static_cast<double>(value));
}

RpcBuilder& RpcBuilder::param(const char* key, bool value)
{
    if (!key) return *this;
    cJSON* params = ensureParamsObject();
    if (!params) return *this;
    cJSON_AddBoolToObject(params, key, value);
    return *this;
}

RpcBuilder& RpcBuilder::param(const char* key, const char* value)
{
    if (!key || !value) return *this;
    cJSON* params = ensureParamsObject();
    if (!params) return *this;
    cJSON_AddStringToObject(params, key, value);
    return *this;
}

RpcBuilder& RpcBuilder::param(const char* key, std::string_view value)
{
    if (!key) return *this;
    std::string tmp(value);
    return param(key, tmp.c_str());
}

RpcBuilder& RpcBuilder::paramsRawJson(std::string_view json)
{
    if (!ensureRoot() || json.empty()) return *this;
    cJSON* parsed = cJSON_ParseWithLength(json.data(), json.size());
    if (!parsed) {
        TB_LOGW("paramsRawJson: parse failed");
        return *this;
    }
    cJSON_DeleteItemFromObject(root_, "params");
    cJSON_AddItemToObject(root_, "params", parsed);
    return *this;
}

RpcBuilder& RpcBuilder::paramsNull()
{
    if (!ensureRoot()) return *this;
    cJSON_DeleteItemFromObject(root_, "params");
    cJSON_AddNullToObject(root_, "params");
    return *this;
}

RpcBuilder& RpcBuilder::paramsEmptyObject()
{
    if (!ensureRoot()) return *this;
    cJSON_DeleteItemFromObject(root_, "params");
    cJSON* empty = cJSON_CreateObject();
    if (!empty) return *this;
    cJSON_AddItemToObject(root_, "params", empty);
    return *this;
}

bool RpcBuilder::empty() const
{
    if (!root_) return true;
    cJSON* method = cJSON_GetObjectItemCaseSensitive(root_, "method");
    return !(cJSON_IsString(method) && method->valuestring && method->valuestring[0] != '\0');
}

std::string RpcBuilder::build() const
{
    if (empty()) return {};
    return printOrEmpty(root_);
}

void RpcBuilder::clear()
{
    if (root_) {
        cJSON_Delete(root_);
        root_ = nullptr;
    }
    root_ = cJSON_CreateObject();
}

// ── RpcReplyBuilder ─────────────────────────────────────────────────────

RpcReplyBuilder::RpcReplyBuilder()
{
    obj_ = cJSON_CreateObject();
}

RpcReplyBuilder::~RpcReplyBuilder()
{
    if (obj_) {
        cJSON_Delete(obj_);
        obj_ = nullptr;
    }
}

RpcReplyBuilder::RpcReplyBuilder(RpcReplyBuilder&& other) noexcept
    : obj_(other.obj_)
{
    other.obj_ = nullptr;
}

RpcReplyBuilder& RpcReplyBuilder::operator=(RpcReplyBuilder&& other) noexcept
{
    if (this != &other) {
        if (obj_) cJSON_Delete(obj_);
        obj_ = other.obj_;
        other.obj_ = nullptr;
    }
    return *this;
}

bool RpcReplyBuilder::ensureObject()
{
    if (!obj_) obj_ = cJSON_CreateObject();
    return obj_ != nullptr;
}

RpcReplyBuilder& RpcReplyBuilder::add(const char* key, double value)
{
    if (!key || !ensureObject()) return *this;
    cJSON_AddNumberToObject(obj_, key, value);
    return *this;
}

RpcReplyBuilder& RpcReplyBuilder::add(const char* key, int64_t value)
{
    return add(key, static_cast<double>(value));
}

RpcReplyBuilder& RpcReplyBuilder::add(const char* key, bool value)
{
    if (!key || !ensureObject()) return *this;
    cJSON_AddBoolToObject(obj_, key, value);
    return *this;
}

RpcReplyBuilder& RpcReplyBuilder::add(const char* key, const char* value)
{
    if (!key || !value || !ensureObject()) return *this;
    cJSON_AddStringToObject(obj_, key, value);
    return *this;
}

RpcReplyBuilder& RpcReplyBuilder::add(const char* key, std::string_view value)
{
    if (!key || !ensureObject()) return *this;
    std::string tmp(value);
    cJSON_AddStringToObject(obj_, key, tmp.c_str());
    return *this;
}

RpcReplyBuilder& RpcReplyBuilder::addRawJson(const char* key, std::string_view json)
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

bool RpcReplyBuilder::empty() const
{
    return !obj_ || obj_->child == nullptr;
}

std::string RpcReplyBuilder::build() const
{
    if (!obj_) return "{}";
    return printOrEmpty(obj_);
}

void RpcReplyBuilder::clear()
{
    if (obj_) {
        cJSON_Delete(obj_);
        obj_ = nullptr;
    }
    obj_ = cJSON_CreateObject();
}

} // namespace thingsboard
