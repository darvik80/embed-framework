/// Host Unity tests for Cogitor IoT AttributeBuilder / request / parsers.

#include "unity.h"

#include "cogitor_iot/attributes.hpp"

#include <cstring>
#include <string>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_cogitor_attribute_request_with_string_id(void)
{
    cogitor::iot::AttributeRequestBuilder req;
    req.id("req-uuid-12345")
       .addReported("firmwareVersion")
       .addDesired("targetTemperature");
    const std::string json = req.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"id\":\"req-uuid-12345\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"reported\":[\"firmwareVersion\"]"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"desired\":[\"targetTemperature\"]"));
}

void test_cogitor_parse_json_id_string(void)
{
    const std::string payload = R"({"id":"req-42","code":0,"message":"ok"})";
    const std::string id = cogitor::iot::parseJsonId(payload);
    TEST_ASSERT_EQUAL_STRING("req-42", id.c_str());
}

void test_cogitor_parse_json_id_numeric_fallback(void)
{
    const std::string payload = R"({"id":42,"code":0,"message":"ok"})";
    const std::string id = cogitor::iot::parseJsonId(payload);
    TEST_ASSERT_EQUAL_STRING("42", id.c_str());
}

void test_cogitor_parse_json_id_missing(void)
{
    const std::string payload = R"({"code":0,"message":"ok"})";
    const std::string id = cogitor::iot::parseJsonId(payload);
    TEST_ASSERT_EQUAL_STRING("", id.c_str());
}

void test_cogitor_parse_attribute_response(void)
{
    const std::string payload =
        R"({"id":"req-7","reported":{"firmwareVersion":"2.1.0"},"desired":{"targetTemperature":25}})";
    const std::string id = cogitor::iot::parseJsonId(payload);
    TEST_ASSERT_EQUAL_STRING("req-7", id.c_str());

    const auto values = cogitor::iot::parseAttributeResponse(payload);
    std::string fw;
    TEST_ASSERT_TRUE(cogitor::iot::attributeGetString(values.reportedJson, "firmwareVersion", fw));
    TEST_ASSERT_EQUAL_STRING("2.1.0", fw.c_str());

    double temp = 0;
    TEST_ASSERT_TRUE(cogitor::iot::attributeGetNumber(values.desiredJson, "targetTemperature", temp));
    TEST_ASSERT_EQUAL_FLOAT(25.0f, static_cast<float>(temp));
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cogitor_attribute_request_with_string_id);
    RUN_TEST(test_cogitor_parse_json_id_string);
    RUN_TEST(test_cogitor_parse_json_id_numeric_fallback);
    RUN_TEST(test_cogitor_parse_json_id_missing);
    RUN_TEST(test_cogitor_parse_attribute_response);
    return UNITY_END();
}
