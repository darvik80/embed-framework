#include "crearts_iot/rpc_registry.hpp"
#include "crearts_iot/crearts_iot_service.hpp"

#include "cJSON.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>

namespace crearts::iot {

static const char* TAG = "CreartsRpcReg";

namespace {

bool sameMethod(const char* a, const char* b)
{
    return a && b && std::strcmp(a, b) == 0;
}

bool isRpcList(const char* method)
{
    return sameMethod(method, kRpcListMethod) || sameMethod(method, "rpc_list");
}

void addDefault(cJSON* spec, const RpcParamDef& p)
{
    if (!spec || !p.hasDefault) return;
    switch (p.type) {
    case RpcParamType::Int:
        cJSON_AddNumberToObject(spec, "default", static_cast<double>(p.defaultInt));
        break;
    case RpcParamType::Bool:
        cJSON_AddBoolToObject(spec, "default", p.defaultBool);
        break;
    case RpcParamType::Number:
        cJSON_AddNumberToObject(spec, "default", p.defaultNumber);
        break;
    case RpcParamType::String:
        cJSON_AddStringToObject(spec, "default", p.defaultString ? p.defaultString : "");
        break;
    }
}

cJSON* paramsSchemaObject(const RpcParamDef* params, uint8_t count)
{
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return nullptr;
    for (uint8_t i = 0; i < count; ++i) {
        const RpcParamDef& p = params[i];
        if (!p.name) continue;
        cJSON* spec = cJSON_CreateObject();
        if (!spec) continue;
        cJSON_AddStringToObject(spec, "type", rpcParamTypeName(p.type));
        cJSON_AddBoolToObject(spec, "required", p.required);
        addDefault(spec, p);
        cJSON_AddItemToObject(obj, p.name, spec);
    }
    return obj;
}

cJSON* requiredArray(const RpcParamDef* params, uint8_t count)
{
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return nullptr;
    for (uint8_t i = 0; i < count; ++i) {
        const RpcParamDef& p = params[i];
        if (!p.name || !p.required) continue;
        cJSON_AddItemToArray(arr, cJSON_CreateString(p.name));
    }
    return arr;
}

cJSON* methodEntry(const char* method,
                   const char* description,
                   const RpcParamDef* params,
                   uint8_t paramCount)
{
    cJSON* item = cJSON_CreateObject();
    if (!item) return nullptr;
    cJSON_AddStringToObject(item, "method", method ? method : "");
    if (description && description[0]) {
        cJSON_AddStringToObject(item, "description", description);
    }
    cJSON* schema = paramsSchemaObject(params, paramCount);
    if (schema) {
        cJSON_AddItemToObject(item, "params", schema);
    } else {
        cJSON_AddObjectToObject(item, "params");
    }
    cJSON* req = requiredArray(params, paramCount);
    if (req) {
        cJSON_AddItemToObject(item, "required", req);
    }
    return item;
}

} // namespace

bool RpcRegistry::add(const char* method,
                      const RpcParamDef* params,
                      uint8_t paramCount,
                      Handler handler,
                      void* ctx,
                      const char* description)
{
    if (!method || !method[0] || !handler) {
        ESP_LOGE(TAG, "add: invalid method/handler");
        return false;
    }
    if (isRpcList(method)) {
        ESP_LOGE(TAG, "add: '%s' is built-in", method);
        return false;
    }
    if (find(method)) {
        ESP_LOGE(TAG, "add: duplicate method '%s'", method);
        return false;
    }
    if (count_ >= kMaxMethods) {
        ESP_LOGE(TAG, "add: registry full (%u)", kMaxMethods);
        return false;
    }

    Entry& e = methods_[count_++];
    e.method = method;
    e.description = description;
    e.params = params;
    e.paramCount = paramCount;
    e.handler = handler;
    e.ctx = ctx;
    ESP_LOGI(TAG, "registered RPC '%s' (%u params)", method, paramCount);
    return true;
}

const RpcRegistry::Entry* RpcRegistry::find(const char* method) const
{
    if (!method) return nullptr;
    for (uint8_t i = 0; i < count_; ++i) {
        if (sameMethod(methods_[i].method, method)) {
            return &methods_[i];
        }
    }
    return nullptr;
}

bool RpcRegistry::dispatch(IotService& iot, const RpcRequest& req) const
{
    if (isRpcList(req.method.c_str())) {
        const std::string json = listJson();
        iot.respondRpc(req.requestId, 0, "ok", json);
        return true;
    }

    const Entry* e = find(req.method.c_str());
    if (!e || !e->handler) {
        return false;
    }

    const RpcParams params(req.params.c_str());
    e->handler(iot, req.requestId, params, e->ctx);
    return true;
}

std::string RpcRegistry::listJson() const
{
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return "[]";

    cJSON_AddItemToArray(arr, methodEntry(kRpcListMethod, "List firmware RPC methods", nullptr, 0));

    for (uint8_t i = 0; i < count_; ++i) {
        const Entry& e = methods_[i];
        cJSON_AddItemToArray(arr, methodEntry(e.method, e.description, e.params, e.paramCount));
    }

    char* printed = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!printed) return "[]";
    std::string out(printed);
    free(printed);
    return out;
}

} // namespace crearts::iot
