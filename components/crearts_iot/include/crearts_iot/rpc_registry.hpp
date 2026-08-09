#pragma once

#include "crearts_iot/rpc_params.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace crearts::iot {

class CreartsIotService;

enum class RpcParamType : uint8_t {
    Int,
    Bool,
    String,
    Number,
};

struct RpcParamDef {
    const char* name = nullptr;
    RpcParamType type = RpcParamType::Int;
    bool required = true;
    bool hasDefault = false;
    int64_t defaultInt = 0;
    bool defaultBool = false;
    double defaultNumber = 0;
    const char* defaultString = nullptr;
};

inline constexpr RpcParamDef rpcInt(const char* name, bool required = true)
{
    return {name, RpcParamType::Int, required, false, 0, false, 0, nullptr};
}
inline constexpr RpcParamDef rpcInt(const char* name, bool required, int64_t defaultValue)
{
    return {name, RpcParamType::Int, required, true, defaultValue, false, 0, nullptr};
}
inline constexpr RpcParamDef rpcBool(const char* name, bool required = true)
{
    return {name, RpcParamType::Bool, required, false, 0, false, 0, nullptr};
}
inline constexpr RpcParamDef rpcBool(const char* name, bool required, bool defaultValue)
{
    return {name, RpcParamType::Bool, required, true, 0, defaultValue, 0, nullptr};
}
inline constexpr RpcParamDef rpcStr(const char* name, bool required = true)
{
    return {name, RpcParamType::String, required, false, 0, false, 0, nullptr};
}
inline constexpr RpcParamDef rpcStr(const char* name, bool required, const char* defaultValue)
{
    return {name, RpcParamType::String, required, true, 0, false, 0, defaultValue};
}
inline constexpr RpcParamDef rpcNum(const char* name, bool required = true)
{
    return {name, RpcParamType::Number, required, false, 0, false, 0, nullptr};
}
inline constexpr RpcParamDef rpcNum(const char* name, bool required, double defaultValue)
{
    return {name, RpcParamType::Number, required, true, 0, false, defaultValue, nullptr};
}

[[nodiscard]] constexpr const char* rpcParamTypeName(RpcParamType type)
{
    switch (type) {
    case RpcParamType::Int: return "int";
    case RpcParamType::Bool: return "bool";
    case RpcParamType::String: return "string";
    case RpcParamType::Number: return "number";
    }
    return "any";
}

/// Built-in discovery method. No params. Returns JSON array of { method, params }.
inline constexpr char kRpcListMethod[] = "rpc-list";

/// Firmware RPC catalog + dispatch. `rpc-list` is built-in (not stored in the table).
class RpcRegistry {
public:
    static constexpr uint8_t kMaxMethods = 24;

    using Handler = void (*)(CreartsIotService& iot,
                             uint32_t requestId,
                             const RpcParams& params,
                             void* ctx);

    bool add(const char* method,
             const RpcParamDef* params,
             uint8_t paramCount,
             Handler handler,
             void* ctx,
             const char* description = nullptr);

    bool add(const char* method, Handler handler, void* ctx, const char* description = nullptr)
    {
        return add(method, nullptr, 0, handler, ctx, description);
    }

    template <size_t N>
    bool add(const char* method,
             const RpcParamDef (&params)[N],
             Handler handler,
             void* ctx,
             const char* description = nullptr)
    {
        static_assert(N <= 255, "too many RPC params");
        return add(method, params, static_cast<uint8_t>(N), handler, ctx, description);
    }

    /// Handle `rpc-list` or a registered method. Returns false → caller should 404.
    [[nodiscard]] bool dispatch(CreartsIotService& iot, const RpcRequest& req) const;

    /// JSON array: `[{ "method":"set_led", "params":{ "offset":{ "type":"int", "required":true, "default":0 }, ... } }, ...]`
    [[nodiscard]] std::string listJson() const;

    [[nodiscard]] uint8_t count() const { return count_; }

private:
    struct Entry {
        const char* method = nullptr;
        const char* description = nullptr;
        const RpcParamDef* params = nullptr;
        uint8_t paramCount = 0;
        Handler handler = nullptr;
        void* ctx = nullptr;
    };

    Entry methods_[kMaxMethods]{};
    uint8_t count_ = 0;

    [[nodiscard]] const Entry* find(const char* method) const;
};

} // namespace crearts::iot
