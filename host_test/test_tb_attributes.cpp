/// Host Unity tests for ThingsBoard AttributeBuilder / request / parsers.

#include "unity.h"

#include "thingsboard/attributes.hpp"

#include <cstring>
#include <string>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_attribute_publish_payload(void)
{
    thingsboard::AttributeBuilder b;
    b.add("firmwareVersion", "2.1.0")
        .add("serialNumber", "SN-4A21F")
        .add("hardwareRevision", "B");
    const std::string json = b.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"firmwareVersion\":\"2.1.0\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"serialNumber\":\"SN-4A21F\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"hardwareRevision\":\"B\""));
}

void test_attribute_request_payload(void)
{
    thingsboard::AttributeRequestBuilder req;
    req.clientKeys("firmwareVersion,serialNumber")
        .addSharedKey("targetTemperature")
        .addSharedKey("enabled");
    const std::string json = req.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"clientKeys\":\"firmwareVersion,serialNumber\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"sharedKeys\":\"targetTemperature,enabled\""));
}

void test_attribute_request_dedupes_keys(void)
{
    thingsboard::AttributeRequestBuilder req;
    req.addClientKey("a").addClientKey("a").clientKeys("b, a");
    const std::string json = req.build();
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"clientKeys\":\"a,b\""));
}

void test_parse_attribute_response_values(void)
{
    const std::string payload =
        R"({"client":{"firmwareVersion":"2.1.0","serialNumber":"SN-4A21F"},"shared":{"targetTemperature":24,"enabled":true}})";

    const auto values = thingsboard::parseAttributeResponse(payload);
    TEST_ASSERT_NOT_NULL(strstr(values.clientJson.c_str(), "\"firmwareVersion\":\"2.1.0\""));
    TEST_ASSERT_NOT_NULL(strstr(values.sharedJson.c_str(), "\"targetTemperature\":24"));

    std::string fw;
    TEST_ASSERT_TRUE(thingsboard::attributeGetString(values.clientJson, "firmwareVersion", fw));
    TEST_ASSERT_EQUAL_STRING("2.1.0", fw.c_str());

    double temp = 0;
    TEST_ASSERT_TRUE(thingsboard::attributeGetNumber(values.sharedJson, "targetTemperature", temp));
    TEST_ASSERT_EQUAL_DOUBLE(24.0, temp);

    bool enabled = false;
    TEST_ASSERT_TRUE(thingsboard::attributeGetBool(values.sharedJson, "enabled", enabled));
    TEST_ASSERT_TRUE(enabled);
}

void test_parse_attribute_update_flat(void)
{
    const std::string payload = R"({"targetTemperature":26})";
    const auto values = thingsboard::parseAttributeUpdate(payload);
    TEST_ASSERT_EQUAL_STRING("{}", values.clientJson.c_str());

    double temp = 0;
    TEST_ASSERT_TRUE(thingsboard::attributeGetNumber(values.sharedJson, "targetTemperature", temp));
    TEST_ASSERT_EQUAL_DOUBLE(26.0, temp);
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_attribute_publish_payload);
    RUN_TEST(test_attribute_request_payload);
    RUN_TEST(test_attribute_request_dedupes_keys);
    RUN_TEST(test_parse_attribute_response_values);
    RUN_TEST(test_parse_attribute_update_flat);
    return UNITY_END();
}
