/// Host Unity tests for ThingsBoard RPC builders and topics.

#include "unity.h"

#include "thingsboard/rpc.hpp"
#include "thingsboard/topics.hpp"

#include <cstring>
#include <string>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_rpc_builder_method_params(void)
{
    thingsboard::RpcBuilder b;
    b.method("setTemperature").param("value", 26);
    const std::string json = b.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"method\":\"setTemperature\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"params\":{"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"value\":26"));
}

void test_rpc_builder_params_empty_object(void)
{
    thingsboard::RpcBuilder b;
    b.method("getSessionLimits").paramsEmptyObject();
    const std::string json = b.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"method\":\"getSessionLimits\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"params\":{}"));
}

void test_rpc_builder_params_raw_json(void)
{
    thingsboard::RpcBuilder b;
    b.method("echo").paramsRawJson(R"({"msg":"hi"})");
    const std::string json = b.build();
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"method\":\"echo\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"msg\":\"hi\""));
}

void test_rpc_builder_empty_without_method(void)
{
    thingsboard::RpcBuilder b;
    b.param("value", 1);
    TEST_ASSERT_TRUE(b.empty());
    TEST_ASSERT_TRUE(b.build().empty());
}

void test_rpc_reply_builder(void)
{
    thingsboard::RpcReplyBuilder reply;
    reply.add("ok", true).add("temperature", 22);
    const std::string json = reply.build();
    TEST_ASSERT_FALSE(json.empty());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"ok\":true"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"temperature\":22"));
}

void test_rpc_topics_short(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Short);
    TEST_ASSERT_EQUAL_STRING("v2/r/req/+", t.rpcRequestSubscribe().c_str());
    TEST_ASSERT_EQUAL_STRING("v2/r/res/+", t.rpcResponseSubscribe().c_str());
    TEST_ASSERT_EQUAL_STRING("v2/r/req/42", t.rpcClientRequest(42).c_str());
    TEST_ASSERT_EQUAL_STRING("v2/r/res/42", t.rpcResponse(42).c_str());
}

void test_rpc_topics_standard(void)
{
    thingsboard::Topics t(thingsboard::TopicStyle::Standard);
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/rpc/request/+", t.rpcRequestSubscribe().c_str());
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/rpc/response/+", t.rpcResponseSubscribe().c_str());
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/rpc/request/7", t.rpcClientRequest(7).c_str());
    TEST_ASSERT_EQUAL_STRING("v1/devices/me/rpc/response/7", t.rpcResponse(7).c_str());
}

void test_rpc_topic_classification(void)
{
    TEST_ASSERT_TRUE(thingsboard::Topics::isRpcRequest("v2/r/req/99"));
    TEST_ASSERT_TRUE(thingsboard::Topics::isRpcRequest("v1/devices/me/rpc/request/99"));
    TEST_ASSERT_FALSE(thingsboard::Topics::isRpcRequest("v2/r/res/99"));

    TEST_ASSERT_TRUE(thingsboard::Topics::isRpcResponse("v2/r/res/99"));
    TEST_ASSERT_TRUE(thingsboard::Topics::isRpcResponse("v1/devices/me/rpc/response/99"));
    TEST_ASSERT_FALSE(thingsboard::Topics::isRpcResponse("v2/r/req/99"));

    TEST_ASSERT_EQUAL_UINT32(99, thingsboard::Topics::parseRpcRequestId("v2/r/req/99"));
    TEST_ASSERT_EQUAL_UINT32(99, thingsboard::Topics::parseTrailingId("v2/r/res/99"));
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rpc_builder_method_params);
    RUN_TEST(test_rpc_builder_params_empty_object);
    RUN_TEST(test_rpc_builder_params_raw_json);
    RUN_TEST(test_rpc_builder_empty_without_method);
    RUN_TEST(test_rpc_reply_builder);
    RUN_TEST(test_rpc_topics_short);
    RUN_TEST(test_rpc_topics_standard);
    RUN_TEST(test_rpc_topic_classification);
    return UNITY_END();
}
