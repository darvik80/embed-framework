/// Host Unity tests for Cogitor IoT RPC parsing and registry.

#include "unity.h"

#include "cogitor_iot/rpc_params.hpp"

#include <cstring>
#include <string>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_parse_rpc_request_string_id(void)
{
    const std::string payload = R"({"id":"rpc-req-99","method":"set_led","params":{"gpio":4,"brightness":128}})";
    cogitor::iot::RpcRequest req{};
    TEST_ASSERT_TRUE(cogitor::iot::parseRpcRequest(payload, req));
    TEST_ASSERT_EQUAL_STRING("rpc-req-99", req.requestId.c_str());
    TEST_ASSERT_EQUAL_STRING("set_led", req.method.c_str());

    cogitor::iot::RpcParams params(req.params.c_str());
    int gpio = 0;
    TEST_ASSERT_TRUE(params.get("gpio", gpio));
    TEST_ASSERT_EQUAL_INT(4, gpio);
    TEST_ASSERT_EQUAL_INT(128, params.getInt("brightness", 0));
}

void test_parse_rpc_request_numeric_id_fallback(void)
{
    const std::string payload = R"({"id":42,"method":"reboot","params":{"delayMs":1000}})";
    cogitor::iot::RpcRequest req{};
    TEST_ASSERT_TRUE(cogitor::iot::parseRpcRequest(payload, req));
    TEST_ASSERT_EQUAL_STRING("42", req.requestId.c_str());
    TEST_ASSERT_EQUAL_STRING("reboot", req.method.c_str());

    cogitor::iot::RpcParams params(req.params.c_str());
    TEST_ASSERT_EQUAL_INT(1000, params.getInt("delayMs", 0));
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_rpc_request_string_id);
    RUN_TEST(test_parse_rpc_request_numeric_id_fallback);
    return UNITY_END();
}
