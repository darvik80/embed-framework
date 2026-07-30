/// Host Unity tests for alicloud::iot::AlicloudCredentials.
/// Compiles mbedtls SHA-256 on host (portable C, no ESP-IDF needed).

#include "unity.h"

#include "alicloud_iot/alicloud_credentials.hpp"

#include <cstring>
#include <string>
#include <string_view>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_create_valid_credentials(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create(
        "a1TestProduct", "myDevice01", "myDeviceSecret123");
    TEST_ASSERT_TRUE(creds.has_value());
    TEST_ASSERT_TRUE(creds->isValid());
}

void test_username_format(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create(
        "a1Prod", "dev1", "secret");
    TEST_ASSERT_TRUE(creds.has_value());
    // username = device&product
    TEST_ASSERT_EQUAL_STRING("dev1&a1Prod", creds->username());
}

void test_uri_format(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create(
        "a1Prod", "dev1", "secret");
    TEST_ASSERT_TRUE(creds.has_value());
    // URI = wss://{product}.iot-as-mqtt.ap-southeast-1.aliyuncs.com
    TEST_ASSERT_EQUAL_STRING(
        "wss://a1Prod.iot-as-mqtt.ap-southeast-1.aliyuncs.com",
        creds->brokerUri());
}

void test_password_is_64_hex_chars(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create(
        "a1Prod", "dev1", "secret");
    TEST_ASSERT_TRUE(creds.has_value());
    const char* pw = creds->password();
    TEST_ASSERT_EQUAL_UINT(64, strlen(pw));
    // All chars must be uppercase hex
    for (const char* p = pw; *p; ++p) {
        TEST_ASSERT_TRUE((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'));
    }
}

void test_password_deterministic(void)
{
    auto a = alicloud::iot::AlicloudCredentials::create("pk", "dn", "ds");
    auto b = alicloud::iot::AlicloudCredentials::create("pk", "dn", "ds");
    TEST_ASSERT_TRUE(a.has_value() && b.has_value());
    TEST_ASSERT_EQUAL_STRING(a->password(), b->password());
}

void test_different_secret_different_password(void)
{
    auto a = alicloud::iot::AlicloudCredentials::create("pk", "dn", "secret1");
    auto b = alicloud::iot::AlicloudCredentials::create("pk", "dn", "secret2");
    TEST_ASSERT_TRUE(a.has_value() && b.has_value());
    TEST_ASSERT_TRUE(strcmp(a->password(), b->password()) != 0);
}

void test_clientid_contains_username(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create(
        "a1Prod", "dev1", "secret");
    TEST_ASSERT_TRUE(creds.has_value());
    // clientId starts with username
    std::string_view cid(creds->clientId());
    TEST_ASSERT_TRUE(cid.starts_with("dev1&a1Prod"));
    // clientId contains signmethod
    TEST_ASSERT_NOT_NULL(strstr(creds->clientId(), "signmethod=hmacsha256"));
}

void test_reject_empty_product(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create("", "dev", "secret");
    TEST_ASSERT_FALSE(creds.has_value());
}

void test_reject_empty_device(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create("prod", "", "secret");
    TEST_ASSERT_FALSE(creds.has_value());
}

void test_reject_empty_secret(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create("prod", "dev", "");
    TEST_ASSERT_FALSE(creds.has_value());
}

void test_product_key_accessor(void)
{
    auto creds = alicloud::iot::AlicloudCredentials::create("myPK", "myDN", "s");
    TEST_ASSERT_TRUE(creds.has_value());
    TEST_ASSERT_EQUAL_STRING("myPK", std::string(creds->productKey()).c_str());
    TEST_ASSERT_EQUAL_STRING("myDN", std::string(creds->deviceName()).c_str());
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_create_valid_credentials);
    RUN_TEST(test_username_format);
    RUN_TEST(test_uri_format);
    RUN_TEST(test_password_is_64_hex_chars);
    RUN_TEST(test_password_deterministic);
    RUN_TEST(test_different_secret_different_password);
    RUN_TEST(test_clientid_contains_username);
    RUN_TEST(test_reject_empty_product);
    RUN_TEST(test_reject_empty_device);
    RUN_TEST(test_reject_empty_secret);
    RUN_TEST(test_product_key_accessor);
    return UNITY_END();
}
