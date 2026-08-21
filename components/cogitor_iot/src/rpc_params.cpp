#include "cogitor_iot/rpc_params.hpp"
#include "cogitor_iot/cogitor_iot_service.hpp"

#include "cJSON.h"

#include <cstdlib>
#include <utility>

namespace cogitor::iot {

namespace {

cJSON* parseObjectOrEmpty(std::string_view json)
{
    if (json.empty()) {
        return cJSON_CreateObject();
    }
    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return cJSON_CreateObject();
    }
    return root;
}

} // namespace

RpcParams::RpcParams(std::string_view json)
    : root_(parseObjectOrEmpty(json))
{}

RpcParams::~RpcParams()
{
    if (root_) cJSON_Delete(root_);
}

RpcParams::RpcParams(RpcParams&& other) noexcept
    : root_(other.root_)
{
    other.root_ = nullptr;
}

RpcParams& RpcParams::operator=(RpcParams&& other) noexcept
{
    if (this != &other) {
        if (root_) cJSON_Delete(root_);
        root_ = other.root_;
        other.root_ = nullptr;
    }
    return *this;
}

bool RpcParams::empty() const
{
    return !root_ || root_->child == nullptr;
}

cJSON* RpcParams::item(const char* key) const
{
    if (!root_ || !key) return nullptr;
    return cJSON_GetObjectItemCaseSensitive(root_, key);
}

bool RpcParams::get(const char* key, int& out) const
{
    int64_t v = 0;
    if (!get(key, v)) return false;
    out = static_cast<int>(v);
    return true;
}

bool RpcParams::get(const char* key, int64_t& out) const
{
    cJSON* it = item(key);
    if (!cJSON_IsNumber(it)) return false;
    out = static_cast<int64_t>(it->valuedouble);
    return true;
}

bool RpcParams::get(const char* key, bool& out) const
{
    cJSON* it = item(key);
    if (!cJSON_IsBool(it)) return false;
    out = cJSON_IsTrue(it);
    return true;
}

bool RpcParams::get(const char* key, std::string& out) const
{
    cJSON* it = item(key);
    if (!cJSON_IsString(it) || !it->valuestring) return false;
    out = it->valuestring;
    return true;
}

int RpcParams::getInt(const char* key, int fallback) const
{
    int v = fallback;
    return get(key, v) ? v : fallback;
}

bool RpcParams::getBool(const char* key, bool fallback) const
{
    bool v = fallback;
    return get(key, v) ? v : fallback;
}

std::string RpcParams::getString(const char* key, const char* fallback) const
{
    std::string v;
    if (get(key, v)) return v;
    return fallback ? std::string(fallback) : std::string{};
}

bool parseRpcRequest(std::string_view payload, RpcRequest& out)
{
    out = RpcRequest{};
    if (payload.empty()) return false;

    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    cJSON* id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (cJSON_IsNumber(id) && id->valuedouble > 0) {
        out.requestId = static_cast<uint32_t>(id->valuedouble);
    }

    cJSON* method = cJSON_GetObjectItemCaseSensitive(root, "method");
    if (cJSON_IsString(method) && method->valuestring) {
        out.method = method->valuestring;
    }

    cJSON* params = cJSON_GetObjectItemCaseSensitive(root, "params");
    if (params) {
        char* printed = cJSON_PrintUnformatted(params);
        if (printed) {
            out.params = printed;
            free(printed);
        }
    }

    cJSON_Delete(root);
    return true;
}

} // namespace cogitor::iot
