#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct cJSON;

namespace crearts::iot {

/// Flat attribute report: {"firmwareVersion":"2.1.0",...}
class AttributeBuilder {
public:
    AttributeBuilder();
    ~AttributeBuilder();

    AttributeBuilder(AttributeBuilder&& other) noexcept;
    AttributeBuilder& operator=(AttributeBuilder&& other) noexcept;

    AttributeBuilder(const AttributeBuilder&) = delete;
    AttributeBuilder& operator=(const AttributeBuilder&) = delete;

    AttributeBuilder& add(const char* key, double value);
    AttributeBuilder& add(const char* key, int64_t value);
    AttributeBuilder& add(const char* key, int value) {
        return add(key, static_cast<int64_t>(value));
    }
    AttributeBuilder& add(const char* key, bool value);
    AttributeBuilder& add(const char* key, const char* value);
    AttributeBuilder& add(const char* key, std::string_view value);
    AttributeBuilder& addRawJson(const char* key, std::string_view json);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::string build() const;
    void clear();

private:
    cJSON* obj_ = nullptr;
    bool ensureObject();
};

/// Attribute request body with reported/desired key arrays.
/// Service injects `"id"` when publishing; optional id() for standalone build.
class AttributeRequestBuilder {
public:
    AttributeRequestBuilder() = default;

    AttributeRequestBuilder& id(uint32_t requestId);
    AttributeRequestBuilder& addReported(std::string_view key);
    AttributeRequestBuilder& addDesired(std::string_view key);
    AttributeRequestBuilder& reportedAll();
    AttributeRequestBuilder& desiredAll();

    [[nodiscard]] bool empty() const {
        return reported_.empty() && desired_.empty();
    }

    [[nodiscard]] std::string build() const;
    void clear();

private:
    uint32_t id_ = 0;
    bool hasId_ = false;
    std::vector<std::string> reported_;
    std::vector<std::string> desired_;

    static void appendUnique(std::vector<std::string>& keys, std::string_view key);
};

struct AttributeValues {
    std::string reportedJson; ///< "{}" if absent
    std::string desiredJson;  ///< "{}" if absent
};

[[nodiscard]] AttributeValues parseAttributeResponse(std::string_view payload);
[[nodiscard]] AttributeValues parseAttributeUpdate(std::string_view payload);

[[nodiscard]] bool attributeGetNumber(std::string_view objectJson, const char* key, double& out);
[[nodiscard]] bool attributeGetBool(std::string_view objectJson, const char* key, bool& out);
[[nodiscard]] bool attributeGetString(std::string_view objectJson, const char* key, std::string& out);

/// Extract numeric `"id"` from a JSON object; returns 0 if missing/invalid.
[[nodiscard]] uint32_t parseJsonId(std::string_view payload);

} // namespace crearts::iot
