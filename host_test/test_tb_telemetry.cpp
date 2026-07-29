/// Host Unity tests for ThingsBoard TelemetryBuilder / TelemetryBatch.

#include "unity.h"

#include "thingsboard/telemetry.hpp"

#include <cstring>
#include <string>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_telemetry_simple_kv(void)
{
    thingsboard::TelemetryBuilder b;
    b.add("temperature", 22.5).add("humidity", 61);
    const std::string json = b.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"temperature\":22.5"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"humidity\":61"));
    TEST_ASSERT_NULL(strstr(json.c_str(), "\"ts\""));
    TEST_ASSERT_NULL(strstr(json.c_str(), "\"values\""));
}

void test_telemetry_with_client_ts(void)
{
    thingsboard::TelemetryBuilder b;
    b.timestampMs(1451649600512LL)
        .add("temperature", 22.5)
        .add("humidity", 61);
    const std::string json = b.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"ts\":1451649600512"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"values\":{"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"temperature\":22.5"));
}

void test_telemetry_batch(void)
{
    thingsboard::TelemetryBatch batch;
    {
        thingsboard::TelemetryBuilder a;
        a.timestampMs(1451649600000LL).add("temperature", 22.5);
        TEST_ASSERT_TRUE(batch.add(std::move(a)));
    }
    {
        thingsboard::TelemetryBuilder b;
        b.timestampMs(1451649601000LL).add("temperature", 22.7);
        TEST_ASSERT_TRUE(batch.add(std::move(b)));
    }
    const std::string json = batch.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_EQUAL_CHAR('[', json.front());
    TEST_ASSERT_EQUAL_CHAR(']', json.back());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "1451649600000"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "1451649601000"));
}

void test_telemetry_batch_requires_ts(void)
{
    thingsboard::TelemetryBatch batch;
    thingsboard::TelemetryBuilder b;
    b.add("temperature", 1.0);
    TEST_ASSERT_FALSE(batch.add(std::move(b)));
    TEST_ASSERT_TRUE(batch.empty());
}

void test_telemetry_types(void)
{
    thingsboard::TelemetryBuilder b;
    b.add("s", "value1")
        .add("flag", true)
        .add("n", static_cast<int64_t>(73))
        .addRawJson("obj", R"({"nested":{"key":"value"}})");
    const std::string json = b.build();
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"s\":\"value1\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"flag\":true"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"n\":73"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"nested\""));
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_telemetry_simple_kv);
    RUN_TEST(test_telemetry_with_client_ts);
    RUN_TEST(test_telemetry_batch);
    RUN_TEST(test_telemetry_batch_requires_ts);
    RUN_TEST(test_telemetry_types);
    return UNITY_END();
}
