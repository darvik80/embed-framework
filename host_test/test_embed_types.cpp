/// Host Unity tests for embed::types (pair, tuple, array, optional, variant).

#include "unity.h"

#include "embed/types.hpp"

#include <type_traits>
#include <utility>

extern "C" {

void setUp(void) {}
void tearDown(void) {}

// ── pair ────────────────────────────────────────────────────────────────

void test_pair_construct_and_access(void)
{
    embed::pair<int, float> p(42, 3.14f);
    TEST_ASSERT_EQUAL_INT(42, p.first);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, p.second);
    TEST_ASSERT_EQUAL_INT(42, p.get<0>());
    TEST_ASSERT_EQUAL_FLOAT(3.14f, p.get<1>());
}

void test_pair_equality(void)
{
    embed::pair<int, int> a(1, 2);
    embed::pair<int, int> b(1, 2);
    embed::pair<int, int> c(1, 3);
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a == c);
}

void test_pair_swap(void)
{
    embed::pair<int, int> a(1, 2);
    embed::pair<int, int> b(3, 4);
    a.swap(b);
    TEST_ASSERT_EQUAL_INT(3, a.first);
    TEST_ASSERT_EQUAL_INT(1, b.first);
}

void test_pair_structured_binding(void)
{
    embed::pair<int, float> p(10, 2.5f);
    auto [x, y] = p;
    TEST_ASSERT_EQUAL_INT(10, x);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, y);
}

void test_pair_std_pair_conversion(void)
{
    embed::pair<int, float> p(7, 1.0f);
    auto sp = static_cast<std::pair<int, float>>(p);
    TEST_ASSERT_EQUAL_INT(7, sp.first);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sp.second);
}

// ── tuple ───────────────────────────────────────────────────────────────

void test_tuple_construct_and_get(void)
{
    embed::tuple<int, float, char> t(1, 2.0f, 'x');
    TEST_ASSERT_EQUAL_INT(1, t.get<0>());
    TEST_ASSERT_EQUAL_FLOAT(2.0f, t.get<1>());
    TEST_ASSERT_EQUAL_CHAR('x', t.get<2>());
    TEST_ASSERT_EQUAL_UINT(3, t.size());
}

void test_tuple_equality(void)
{
    embed::tuple<int, int> a(1, 2);
    embed::tuple<int, int> b(1, 2);
    embed::tuple<int, int> c(2, 1);
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a == c);
}

void test_tuple_swap(void)
{
    embed::tuple<int, int> a(1, 2);
    embed::tuple<int, int> b(3, 4);
    a.swap(b);
    TEST_ASSERT_EQUAL_INT(3, a.get<0>());
    TEST_ASSERT_EQUAL_INT(1, b.get<0>());
}

// ── array ───────────────────────────────────────────────────────────────

void test_array_access_and_size(void)
{
    embed::array<int, 4> arr;
    arr.fill(7);
    TEST_ASSERT_EQUAL_UINT(4, arr.size());
    TEST_ASSERT_FALSE(arr.empty());
    for (std::size_t i = 0; i < arr.size(); ++i) {
        TEST_ASSERT_EQUAL_INT(7, arr[i]);
    }
}

void test_array_iteration(void)
{
    embed::array<int, 3> arr;
    arr[0] = 10; arr[1] = 20; arr[2] = 30;
    int sum = 0;
    for (int v : arr) sum += v;
    TEST_ASSERT_EQUAL_INT(60, sum);
}

void test_array_equality(void)
{
    embed::array<int, 2> a; a[0] = 1; a[1] = 2;
    embed::array<int, 2> b; b[0] = 1; b[1] = 2;
    embed::array<int, 2> c; c[0] = 1; c[1] = 3;
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a == c);
}

void test_array_std_conversion(void)
{
    std::array<int, 3> src = {4, 5, 6};
    embed::array<int, 3> arr(src);
    TEST_ASSERT_EQUAL_INT(5, arr[1]);
    auto back = static_cast<std::array<int, 3>>(arr);
    TEST_ASSERT_EQUAL_INT(6, back[2]);
}

// ── optional ────────────────────────────────────────────────────────────

void test_optional_empty_by_default(void)
{
    embed::optional<int> o;
    TEST_ASSERT_FALSE(o.has_value());
    TEST_ASSERT_FALSE(static_cast<bool>(o));
    TEST_ASSERT_EQUAL_INT(99, o.value_or(99));
}

void test_optional_with_value(void)
{
    embed::optional<int> o(42);
    TEST_ASSERT_TRUE(o.has_value());
    TEST_ASSERT_EQUAL_INT(42, o.value());
    TEST_ASSERT_EQUAL_INT(42, *o);
}

void test_optional_emplace_and_reset(void)
{
    embed::optional<int> o;
    o.emplace(10);
    TEST_ASSERT_TRUE(o.has_value());
    TEST_ASSERT_EQUAL_INT(10, *o);
    o.reset();
    TEST_ASSERT_FALSE(o.has_value());
}

void test_optional_assign(void)
{
    embed::optional<int> o;
    o = 55;
    TEST_ASSERT_TRUE(o.has_value());
    TEST_ASSERT_EQUAL_INT(55, *o);
    o = embed::optional<int>::nullopt;
    TEST_ASSERT_FALSE(o.has_value());
}

void test_optional_equality(void)
{
    embed::optional<int> a(1);
    embed::optional<int> b(1);
    embed::optional<int> c(2);
    embed::optional<int> d;
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a == c);
    TEST_ASSERT_FALSE(a == d);
    TEST_ASSERT_TRUE(d == embed::optional<int>::nullopt);
}

// ── variant ─────────────────────────────────────────────────────────────

void test_variant_construct_and_index(void)
{
    embed::variant<int, float, char> v(3.14f);
    TEST_ASSERT_EQUAL_UINT(1, v.index());
    TEST_ASSERT_TRUE(v.holds_alternative<float>());
    TEST_ASSERT_FALSE(v.holds_alternative<int>());
    TEST_ASSERT_EQUAL_FLOAT(3.14f, v.get<1>());
}

void test_variant_assign(void)
{
    embed::variant<int, float> v(1);
    TEST_ASSERT_EQUAL_UINT(0, v.index());
    v = 2.0f;
    TEST_ASSERT_EQUAL_UINT(1, v.index());
    TEST_ASSERT_EQUAL_FLOAT(2.0f, v.get<1>());
}

void test_variant_get_if(void)
{
    embed::variant<int, float> v(42);
    int* ip = v.get_if<int>();
    float* fp = v.get_if<float>();
    TEST_ASSERT_NOT_NULL(ip);
    TEST_ASSERT_EQUAL_INT(42, *ip);
    TEST_ASSERT_NULL(fp);
}

void test_variant_visit(void)
{
    embed::variant<int, float> v(7);
    int result = 0;
    v.visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int>) result = val * 2;
        else result = static_cast<int>(val);
    });
    TEST_ASSERT_EQUAL_INT(14, result);
}

void test_variant_equality(void)
{
    embed::variant<int, float> a(10);
    embed::variant<int, float> b(10);
    embed::variant<int, float> c(20);
    embed::variant<int, float> d(10.0f);
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_TRUE(a != c);
    TEST_ASSERT_TRUE(a != d); // different index
}

void test_variant_valueless(void)
{
    embed::variant<int, float> v;
    TEST_ASSERT_TRUE(v.valueless_by_exception());
    TEST_ASSERT_TRUE(v.index() == embed::variant<int, float>::npos);
}

// ── trivially copyable checks ───────────────────────────────────────────

void test_all_trivially_copyable(void)
{
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<embed::pair<int, float>>);
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<embed::tuple<int, float, char>>);
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<embed::array<int, 8>>);
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<embed::optional<int>>);
    TEST_ASSERT_TRUE(std::is_trivially_copyable_v<embed::variant<int, float, char>>);
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    // pair
    RUN_TEST(test_pair_construct_and_access);
    RUN_TEST(test_pair_equality);
    RUN_TEST(test_pair_swap);
    RUN_TEST(test_pair_structured_binding);
    RUN_TEST(test_pair_std_pair_conversion);
    // tuple
    RUN_TEST(test_tuple_construct_and_get);
    RUN_TEST(test_tuple_equality);
    RUN_TEST(test_tuple_swap);
    // array
    RUN_TEST(test_array_access_and_size);
    RUN_TEST(test_array_iteration);
    RUN_TEST(test_array_equality);
    RUN_TEST(test_array_std_conversion);
    // optional
    RUN_TEST(test_optional_empty_by_default);
    RUN_TEST(test_optional_with_value);
    RUN_TEST(test_optional_emplace_and_reset);
    RUN_TEST(test_optional_assign);
    RUN_TEST(test_optional_equality);
    // variant
    RUN_TEST(test_variant_construct_and_index);
    RUN_TEST(test_variant_assign);
    RUN_TEST(test_variant_get_if);
    RUN_TEST(test_variant_visit);
    RUN_TEST(test_variant_equality);
    RUN_TEST(test_variant_valueless);
    // meta
    RUN_TEST(test_all_trivially_copyable);
    return UNITY_END();
}
