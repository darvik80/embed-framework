#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct cJSON;

namespace crearts::iot {

struct RpcRequest;

/// One parse of an RPC `params` JSON object. Missing/invalid JSON → empty object.
class RpcParams {
public:
    explicit RpcParams(std::string_view json);
    ~RpcParams();

    RpcParams(RpcParams&& other) noexcept;
    RpcParams& operator=(RpcParams&& other) noexcept;
    RpcParams(const RpcParams&) = delete;
    RpcParams& operator=(const RpcParams&) = delete;

    [[nodiscard]] bool empty() const;

    bool get(const char* key, int& out) const;
    bool get(const char* key, int64_t& out) const;
    bool get(const char* key, bool& out) const;
    bool get(const char* key, std::string& out) const;

    [[nodiscard]] int getInt(const char* key, int fallback = 0) const;
    [[nodiscard]] bool getBool(const char* key, bool fallback = false) const;
    [[nodiscard]] std::string getString(const char* key, const char* fallback = "") const;

private:
    cJSON* root_ = nullptr;

    cJSON* item(const char* key) const;
};

/// Fill `out` from a full RPC request body (`id` / `method` / `params`).
/// Returns false if the payload is not a JSON object.
[[nodiscard]] bool parseRpcRequest(std::string_view payload, RpcRequest& out);

} // namespace crearts::iot
