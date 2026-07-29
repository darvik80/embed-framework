/// Host-side Unity tests for embed primitives (no ESP-IDF, no device).
/// Build & run:
///   cmake -S host_test -B host_test/build
///   cmake --build host_test/build
///   ctest --test-dir host_test/build --output-on-failure

#include "unity.h"

#include "embed/string.hpp"
#include "embed/message.hpp"
#include "embed/config.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

struct SampleMsg {
    uint32_t id;
    embed::string<31> name;
};
static_assert(embed::Message<SampleMsg>);

} // namespace

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_string_default_is_empty(void)
{
    embed::string<15> s;
    TEST_ASSERT_TRUE(s.empty());
    TEST_ASSERT_EQUAL_UINT(0, s.size());
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
    TEST_ASSERT_EQUAL_UINT(15, s.capacity());
}

void test_string_truncates_to_capacity(void)
{
    embed::string<7> s("abcdefghijklmnop");
    TEST_ASSERT_EQUAL_UINT(7, s.size());
    TEST_ASSERT_EQUAL_STRING("abcdefg", s.c_str());
}

void test_string_assign_respects_capacity(void)
{
    embed::string<4> s;
    s.assign("0123456789", 10);
    TEST_ASSERT_EQUAL_UINT(4, s.size());
    TEST_ASSERT_EQUAL_STRING("0123", s.c_str());
}

void test_string_starts_with_and_find(void)
{
    embed::string<31> s("mqtt/topic/data");
    TEST_ASSERT_TRUE(s.starts_with("mqtt/"));
    TEST_ASSERT_FALSE(s.starts_with("http"));
    TEST_ASSERT_EQUAL_UINT(4, s.find('/'));
    TEST_ASSERT_TRUE(s.contains('t'));
}

void test_message_concept_pod_string(void)
{
    SampleMsg msg{};
    msg.id = 42;
    msg.name = "ok";
    TEST_ASSERT_TRUE(sizeof(msg) <= EMBED_MAX_EVENT_DATA_SIZE);
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<SampleMsg>);
    TEST_ASSERT_TRUE(std::is_standard_layout_v<SampleMsg>);
    TEST_ASSERT_EQUAL_STRING("ok", msg.name.c_str());
    TEST_ASSERT_EQUAL_UINT32(42, msg.id);
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_string_default_is_empty);
    RUN_TEST(test_string_truncates_to_capacity);
    RUN_TEST(test_string_assign_respects_capacity);
    RUN_TEST(test_string_starts_with_and_find);
    RUN_TEST(test_message_concept_pod_string);
    return UNITY_END();
}
