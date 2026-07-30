/// Host Unity tests for ThingsBoard Topics (full coverage: telemetry, attributes, claim).

#include "unity.h"

#include "thingsboard/topics.hpp"

#include <string>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

// ── Short style ─────────────────────────────────────────────────────────

void test_short_telemetry(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Short);
    TEST_ASSERT_EQUAL_STRING("v2/t", t.telemetry().c_str());
}

void test_short_attributes(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Short);
    TEST_ASSERT_EQUAL_STRING("v2/a", t.attributes().c_str());
}

void test_short_attributes_request(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Short);
    TEST_ASSERT_EQUAL_STRING("v2/a/req/5", t.attributesRequest(5).c_str());
}

void test_short_attributes_response_subscribe(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Short);
    TEST_ASSERT_EQUAL_STRING("v2/a/res/+", t.attributesResponseSubscribe().c_str());
}

void test_short_claim(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Short);
    TEST_ASSERT_EQUAL_STRING("v2/c", t.claim().c_str());
}

// ── Standard style ──────────────────────────────────────────────────────

void test_standard_telemetry(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/telemetry", t.telemetry().c_str());
}

void test_standard_attributes(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/attributes", t.attributes().c_str());
}

void test_standard_attributes_request(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/attributes/request/3", t.attributesRequest(3).c_str());
}

void test_standard_attributes_response_subscribe(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/attributes/response/+", t.attributesResponseSubscribe().c_str());
}

void test_standard_claim(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/claim", t.claim().c_str());
}

// ── Classification ──────────────────────────────────────────────────────

void test_is_attribute_update(void)
{
    TEST_ASSERT_TRUE(thingsboard::Topics::isAttributeUpdate("v2/a"));
    TEST_ASSERT_TRUE(thingsboard::Topics::isAttributeUpdate("v1/devices/me/attributes"));
    TEST_ASSERT_FALSE(thingsboard::Topics::isAttributeUpdate("v2/a/res/1"));
    TEST_ASSERT_FALSE(thingsboard::Topics::isAttributeUpdate("v2/t"));
}

void test_is_attribute_response(void)
{
    TEST_ASSERT_TRUE(thingsboard::Topics::isAttributeResponse("v2/a/res/7"));
    TEST_ASSERT_TRUE(thingsboard::Topics::isAttributeResponse("v1/devices/me/attributes/response/7"));
    TEST_ASSERT_FALSE(thingsboard::Topics::isAttributeResponse("v2/a"));
    TEST_ASSERT_FALSE(thingsboard::Topics::isAttributeResponse("v2/r/req/7"));
}

void test_parse_trailing_id_edge_cases(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, thingsboard::Topics::parseTrailingId(""));
    TEST_ASSERT_EQUAL_UINT32(0, thingsboard::Topics::parseTrailingId("no/slash/abc"));
    TEST_ASSERT_EQUAL_UINT32(0, thingsboard::Topics::parseTrailingId("ends/with/"));
    TEST_ASSERT_EQUAL_UINT32(123, thingsboard::Topics::parseTrailingId("a/b/123"));
}

void test_parse_rpc_request_id(void)
{
    TEST_ASSERT_EQUAL_UINT32(42, thingsboard::Topics::parseRpcRequestId("v2/r/req/42"));
    TEST_ASSERT_EQUAL_UINT32(7, thingsboard::Topics::parseRpcRequestId("v1/devices/me/rpc/request/7"));
    TEST_ASSERT_EQUAL_UINT32(0, thingsboard::Topics::parseRpcRequestId("v2/r/res/42"));
    TEST_ASSERT_EQUAL_UINT32(0, thingsboard::Topics::parseRpcRequestId("v2/a/req/42"));
}

void test_style_accessor(void)
{
    thingsboard::Topics s(thingsboard::TopicStyle::Short);
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL(thingsboard::TopicStyle::Short, s.style());
    TEST_ASSERT_EQUAL(thingsboard::TopicStyle::Standard, t.style());
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    // Short
    RUN_TEST(test_short_telemetry);
    RUN_TEST(test_short_attributes);
    RUN_TEST(test_short_attributes_request);
    RUN_TEST(test_short_attributes_response_subscribe);
    RUN_TEST(test_short_claim);
    // Standard
    RUN_TEST(test_standard_telemetry);
    RUN_TEST(test_standard_attributes);
    RUN_TEST(test_standard_attributes_request);
    RUN_TEST(test_standard_attributes_response_subscribe);
    RUN_TEST(test_standard_claim);
    // Classification
    RUN_TEST(test_is_attribute_update);
    RUN_TEST(test_is_attribute_response);
    RUN_TEST(test_parse_trailing_id_edge_cases);
    RUN_TEST(test_parse_rpc_request_id);
    RUN_TEST(test_style_accessor);
    return UNITY_END();
}
