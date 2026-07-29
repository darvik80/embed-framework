#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct cJSON;

namespace thingsboard {

/// Builds client-side / echo RPC request payloads.
///
/// Format: {"method":"setTemperature","params":{"value":26}}
/// See https://thingsboard.io/docs/reference/mqtt-api/rpc/
///
/// Move-only (owns cJSON).
class RpcBuilder {
public:
    RpcBuilder();
    ~RpcBuilder();

    RpcBuilder(RpcBuilder&& other) noexcept;
    RpcBuilder& operator=(RpcBuilder&& other) noexcept;

    RpcBuilder(const RpcBuilder&) = delete;
    RpcBuilder& operator=(const RpcBuilder&) = delete;

    RpcBuilder& method(const char* name);
    RpcBuilder& method(std::string_view name);

    /// Ensure params is a JSON object and add a field.
    RpcBuilder& param(const char* key, double value);
    RpcBuilder& param(const char* key, int64_t value);
    RpcBuilder& param(const char* key, int value) { return param(key, static_cast<int64_t>(value)); }
    RpcBuilder& param(const char* key, bool value);
    RpcBuilder& param(const char* key, const char* value);
    RpcBuilder& param(const char* key, std::string_view value);

    /// Replace params with parsed JSON (object/array/primitive).
    RpcBuilder& paramsRawJson(std::string_view json);

    /// Set params to JSON null.
    RpcBuilder& paramsNull();

    /// Set params to empty object {}.
    RpcBuilder& paramsEmptyObject();

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::string build() const;
    void clear();

private:
    cJSON* root_ = nullptr;

    bool ensureRoot();
    cJSON* ensureParamsObject();
};

/// Builds a JSON object used as a two-way server-side RPC reply body.
class RpcReplyBuilder {
public:
    RpcReplyBuilder();
    ~RpcReplyBuilder();

    RpcReplyBuilder(RpcReplyBuilder&& other) noexcept;
    RpcReplyBuilder& operator=(RpcReplyBuilder&& other) noexcept;

    RpcReplyBuilder(const RpcReplyBuilder&) = delete;
    RpcReplyBuilder& operator=(const RpcReplyBuilder&) = delete;

    RpcReplyBuilder& add(const char* key, double value);
    RpcReplyBuilder& add(const char* key, int64_t value);
    RpcReplyBuilder& add(const char* key, int value) { return add(key, static_cast<int64_t>(value)); }
    RpcReplyBuilder& add(const char* key, bool value);
    RpcReplyBuilder& add(const char* key, const char* value);
    RpcReplyBuilder& add(const char* key, std::string_view value);
    RpcReplyBuilder& addRawJson(const char* key, std::string_view json);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::string build() const;
    void clear();

private:
    cJSON* obj_ = nullptr;
    bool ensureObject();
};

} // namespace thingsboard
