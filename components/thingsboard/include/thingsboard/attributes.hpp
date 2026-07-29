#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct cJSON;

namespace thingsboard {

/// Builds client-side attribute publish payloads.
///
/// Topic: v2/a  (or v1/devices/me/attributes)
/// Example: {"firmwareVersion":"2.1.0","serialNumber":"SN-4A21F"}
///
/// See https://thingsboard.io/docs/reference/mqtt-api/attributes/
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
    AttributeBuilder& add(const char* key, int value) { return add(key, static_cast<int64_t>(value)); }
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

/// Builds attribute request payloads for v2/a/req/$id.
///
/// Example: {"clientKeys":"firmwareVersion,serialNumber","sharedKeys":"targetTemperature,enabled"}
class AttributeRequestBuilder {
public:
    AttributeRequestBuilder() = default;

    AttributeRequestBuilder& clientKeys(std::string_view commaSeparated);
    AttributeRequestBuilder& sharedKeys(std::string_view commaSeparated);

    AttributeRequestBuilder& addClientKey(std::string_view key);
    AttributeRequestBuilder& addSharedKey(std::string_view key);

    [[nodiscard]] bool empty() const {
        return clientKeys_.empty() && sharedKeys_.empty();
    }

    [[nodiscard]] std::string build() const;
    void clear();

private:
    std::vector<std::string> clientKeys_;
    std::vector<std::string> sharedKeys_;

    static void appendUnique(std::vector<std::string>& keys, std::string_view key);
    static void appendCsv(std::vector<std::string>& keys, std::string_view csv);
    static std::string joinCsv(const std::vector<std::string>& keys);
};

/// Parsed attribute response / shared-update payload.
///
/// Response shape:
///   {"client":{...},"shared":{...}}
/// Shared update push (subscribe v2/a) is a flat object of changed keys —
/// exposed via sharedJson (entire payload) and empty clientJson.
struct AttributeValues {
    std::string clientJson;  ///< "{}" if absent
    std::string sharedJson;  ///< "{}" if absent
};

/// Parse get-attributes response JSON into client/shared objects.
[[nodiscard]] AttributeValues parseAttributeResponse(std::string_view payload);

/// Parse shared-attribute push (flat object) — sharedJson = payload object, client empty.
[[nodiscard]] AttributeValues parseAttributeUpdate(std::string_view payload);

/// Read a number/bool/string from a JSON object string (e.g. values.sharedJson).
[[nodiscard]] bool attributeGetNumber(std::string_view objectJson, const char* key, double& out);
[[nodiscard]] bool attributeGetBool(std::string_view objectJson, const char* key, bool& out);
[[nodiscard]] bool attributeGetString(std::string_view objectJson, const char* key, std::string& out);

} // namespace thingsboard
