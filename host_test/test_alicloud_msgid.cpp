/// Host Unity tests for alicloud::iot::MessageIdGenerator.

#include "unity.h"

#include "alicloud_iot/message_id_generator.hpp"

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_monotonic_increment(void)
{
    uint32_t a = alicloud::iot::MessageIdGenerator::generate();
    uint32_t b = alicloud::iot::MessageIdGenerator::generate();
    uint32_t c = alicloud::iot::MessageIdGenerator::generate();
    TEST_ASSERT_EQUAL_UINT32(a + 1, b);
    TEST_ASSERT_EQUAL_UINT32(b + 1, c);
}

void test_never_zero(void)
{
    // generate() pre-increments from 0, so first call returns >= 1
    uint32_t id = alicloud::iot::MessageIdGenerator::generate();
    TEST_ASSERT_GREATER_THAN_UINT32(0, id);
}

void test_sequence_gap_free(void)
{
    uint32_t first = alicloud::iot::MessageIdGenerator::generate();
    for (int i = 1; i <= 100; ++i) {
        uint32_t next = alicloud::iot::MessageIdGenerator::generate();
        TEST_ASSERT_EQUAL_UINT32(first + static_cast<uint32_t>(i), next);
    }
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_monotonic_increment);
    RUN_TEST(test_never_zero);
    RUN_TEST(test_sequence_gap_free);
    return UNITY_END();
}
