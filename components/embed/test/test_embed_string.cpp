#include "unity.h"
#include "embed/string.hpp"
#include "embed/message.hpp"
#include "embed/config.hpp"

#include <cstring>

namespace {

struct SampleMsg {
    uint32_t id;
    embed::string<31> name;
};
static_assert(embed::Message<SampleMsg>);

} // namespace

TEST_CASE("embed::string default is empty", "[embed][string]")
{
    embed::string<15> s;
    TEST_ASSERT_TRUE(s.empty());
    TEST_ASSERT_EQUAL_UINT(0, s.size());
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
    TEST_ASSERT_EQUAL_UINT(15, s.capacity());
}

TEST_CASE("embed::string truncates to capacity", "[embed][string]")
{
    embed::string<7> s("abcdefghijklmnop");
    TEST_ASSERT_EQUAL_UINT(7, s.size());
    TEST_ASSERT_EQUAL_STRING("abcdefg", s.c_str());
}

TEST_CASE("embed::string assign respects capacity", "[embed][string]")
{
    embed::string<4> s;
    s.assign("0123456789", 10);
    TEST_ASSERT_EQUAL_UINT(4, s.size());
    TEST_ASSERT_EQUAL_STRING("0123", s.c_str());
}

TEST_CASE("embed::string starts_with and find", "[embed][string]")
{
    embed::string<31> s("mqtt/topic/data");
    TEST_ASSERT_TRUE(s.starts_with("mqtt/"));
    TEST_ASSERT_FALSE(s.starts_with("http"));
    TEST_ASSERT_EQUAL_UINT(4, s.find('/'));
    TEST_ASSERT_TRUE(s.contains('t'));
}

TEST_CASE("Message concept accepts POD + embed::string", "[embed][message]")
{
    SampleMsg msg{};
    msg.id = 42;
    msg.name = "ok";
    TEST_ASSERT_TRUE(sizeof(msg) <= EMBED_MAX_EVENT_DATA_SIZE);
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<SampleMsg>);
    TEST_ASSERT_EQUAL_STRING("ok", msg.name.c_str());
    TEST_ASSERT_EQUAL_UINT32(42, msg.id);
}
