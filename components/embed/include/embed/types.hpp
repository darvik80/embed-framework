#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace embed {

// ── Concept ─────────────────────────────────────────────────────────────

/// Concept that constrains a type to be trivially copyable.
template<typename T>
concept trivially_copyable = std::is_trivially_copyable_v<T>;

// ── pair ────────────────────────────────────────────────────────────────

/// Trivially-copyable pair. Both element types must be trivially copyable.
template<trivially_copyable First, trivially_copyable Second>
struct pair {
    First  first{};
    Second second{};

    pair()                       = default;
    pair(const pair&)            = default;
    pair& operator=(const pair&) = default;
    pair(pair&&)                 = default;
    pair& operator=(pair&&)      = default;
    ~pair()                      = default;

    constexpr pair(const First& firstValue, const Second& secondValue)
        : first(firstValue), second(secondValue) {}

    constexpr pair(const std::pair<First, Second>& source)
        : first(source.first), second(source.second) {}

    [[nodiscard]] constexpr explicit operator std::pair<First, Second>() const {
        return {first, second};
    }

    constexpr void swap(pair& other) noexcept {
        pair temp = *this;
        *this = other;
        other = temp;
    }

    template<std::size_t Index>
    [[nodiscard]] constexpr auto& get() noexcept {
        if constexpr (Index == 0) return first;
        else { static_assert(Index == 1); return second; }
    }

    template<std::size_t Index>
    [[nodiscard]] constexpr const auto& get() const noexcept {
        if constexpr (Index == 0) return first;
        else { static_assert(Index == 1); return second; }
    }

    [[nodiscard]] constexpr bool operator==(const pair&) const = default;
    [[nodiscard]] constexpr auto operator<=>(const pair&) const = default;
};

static_assert(std::is_trivially_copyable_v<pair<int, float>>,
              "embed::pair must be trivially copyable");

// ── tuple ───────────────────────────────────────────────────────────────

namespace detail {

template<std::size_t Index, trivially_copyable... Types>
struct tuple_storage;

template<std::size_t Index>
struct tuple_storage<Index> {
    constexpr bool operator==(const tuple_storage&) const = default;
};

template<std::size_t Index, trivially_copyable Head, trivially_copyable... Tail>
struct tuple_storage<Index, Head, Tail...> {
    Head                             _head{};
    tuple_storage<Index + 1, Tail...> _tail{};

    tuple_storage()                                = default;
    tuple_storage(const tuple_storage&)            = default;
    tuple_storage& operator=(const tuple_storage&) = default;
    tuple_storage(tuple_storage&&)                 = default;
    tuple_storage& operator=(tuple_storage&&)      = default;
    ~tuple_storage()                               = default;

    constexpr tuple_storage(const Head& head, const Tail&... tail)
        : _head(head), _tail(tail...) {}

    constexpr bool operator==(const tuple_storage&) const = default;
};

template<std::size_t Target, std::size_t Current, trivially_copyable Head, trivially_copyable... Tail>
constexpr auto& tuple_get(tuple_storage<Current, Head, Tail...>& storage) noexcept {
    if constexpr (Target == Current) return storage._head;
    else return tuple_get<Target>(storage._tail);
}

template<std::size_t Target, std::size_t Current, trivially_copyable Head, trivially_copyable... Tail>
constexpr const auto& tuple_get(const tuple_storage<Current, Head, Tail...>& storage) noexcept {
    if constexpr (Target == Current) return storage._head;
    else return tuple_get<Target>(storage._tail);
}

} // namespace detail

/// Trivially-copyable tuple. Every element type must be trivially copyable.
template<trivially_copyable... Types>
struct tuple {
    detail::tuple_storage<0, Types...> _storage{};

    tuple()                        = default;
    tuple(const tuple&)            = default;
    tuple& operator=(const tuple&) = default;
    tuple(tuple&&)                 = default;
    tuple& operator=(tuple&&)      = default;
    ~tuple()                       = default;

    constexpr explicit tuple(const Types&... values) : _storage(values...) {}

    template<std::size_t Index>
    [[nodiscard]] constexpr auto& get() noexcept {
        return detail::tuple_get<Index>(_storage);
    }

    template<std::size_t Index>
    [[nodiscard]] constexpr const auto& get() const noexcept {
        return detail::tuple_get<Index>(_storage);
    }

    [[nodiscard]] static constexpr std::size_t size() noexcept { return sizeof...(Types); }

    constexpr void swap(tuple& other) noexcept {
        tuple temp = *this;
        *this = other;
        other = temp;
    }

    [[nodiscard]] constexpr bool operator==(const tuple&) const = default;
};

static_assert(std::is_trivially_copyable_v<tuple<int, float, char>>,
              "embed::tuple must be trivially copyable");

// ── array ───────────────────────────────────────────────────────────────

/// Trivially-copyable fixed-size array.
template<trivially_copyable T, std::size_t N>
struct array {
    T _data[N]{};

    array()                        = default;
    array(const array&)            = default;
    array& operator=(const array&) = default;
    array(array&&)                 = default;
    array& operator=(array&&)      = default;
    ~array()                       = default;

    constexpr array(const std::array<T, N>& source) {
        std::copy_n(source.data(), N, _data);
    }

    [[nodiscard]] constexpr T&       operator[](std::size_t index)       noexcept { return _data[index]; }
    [[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept { return _data[index]; }

    [[nodiscard]] constexpr T*       data()       noexcept { return _data; }
    [[nodiscard]] constexpr const T* data() const noexcept { return _data; }
    [[nodiscard]] static constexpr std::size_t size()     noexcept { return N; }
    [[nodiscard]] constexpr bool empty() const noexcept { return N == 0; }

    constexpr void fill(const T& value) noexcept {
        for (std::size_t i = 0; i < N; ++i) _data[i] = value;
    }

    [[nodiscard]] constexpr T*       begin()       noexcept { return _data; }
    [[nodiscard]] constexpr const T* begin() const noexcept { return _data; }
    [[nodiscard]] constexpr T*       end()         noexcept { return _data + N; }
    [[nodiscard]] constexpr const T* end()   const noexcept { return _data + N; }

    [[nodiscard]] constexpr bool operator==(const array&) const = default;
    [[nodiscard]] constexpr auto operator<=>(const array&) const = default;

    [[nodiscard]] constexpr explicit operator std::array<T, N>() const {
        std::array<T, N> result{};
        std::copy_n(_data, N, result.data());
        return result;
    }
};

static_assert(std::is_trivially_copyable_v<array<int, 4>>,
              "embed::array must be trivially copyable");

// ── optional ────────────────────────────────────────────────────────────

/// Trivially-copyable optional.
template<trivially_copyable T>
struct optional {
    T    _value{};
    bool _has_value{false};

    optional()                           = default;
    optional(const optional&)            = default;
    optional& operator=(const optional&) = default;
    optional(optional&&)                 = default;
    optional& operator=(optional&&)      = default;
    ~optional()                          = default;

    constexpr optional(const T& value) : _value(value), _has_value(true) {}

    struct nullopt_t { constexpr explicit nullopt_t() = default; };
    static constexpr nullopt_t nullopt{};

    constexpr optional(nullopt_t) noexcept : _value{}, _has_value{false} {}

    constexpr optional& operator=(nullopt_t) noexcept { reset(); return *this; }

    constexpr void reset() noexcept { _value = T{}; _has_value = false; }

    constexpr T& emplace(const T& value) noexcept {
        _value = value; _has_value = true; return _value;
    }

    constexpr optional& operator=(const T& value) noexcept {
        _value = value; _has_value = true; return *this;
    }

    [[nodiscard]] constexpr bool     has_value()  const noexcept { return _has_value; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _has_value; }

    [[nodiscard]] constexpr const T& value() const noexcept { return _value; }
    [[nodiscard]] constexpr T&       value()       noexcept { return _value; }

    [[nodiscard]] constexpr const T& operator*() const noexcept { return _value; }
    [[nodiscard]] constexpr T&       operator*()       noexcept { return _value; }

    [[nodiscard]] constexpr T value_or(const T& fallback) const noexcept {
        return _has_value ? _value : fallback;
    }

    [[nodiscard]] constexpr bool operator==(const optional& other) const noexcept {
        if (_has_value != other._has_value) return false;
        if (!_has_value) return true;
        return _value == other._value;
    }

    [[nodiscard]] constexpr bool operator==(nullopt_t) const noexcept { return !_has_value; }
};

static_assert(std::is_trivially_copyable_v<optional<int>>,
              "embed::optional must be trivially copyable");

// ── variant ─────────────────────────────────────────────────────────────

namespace detail {

template<typename... Types>
struct max_sizeof;

template<typename Head>
struct max_sizeof<Head> : std::integral_constant<std::size_t, sizeof(Head)> {};

template<typename Head, typename... Tail>
struct max_sizeof<Head, Tail...>
    : std::integral_constant<std::size_t,
          (sizeof(Head) > max_sizeof<Tail...>::value) ? sizeof(Head) : max_sizeof<Tail...>::value> {};

template<typename... Types>
struct max_alignof;

template<typename Head>
struct max_alignof<Head> : std::integral_constant<std::size_t, alignof(Head)> {};

template<typename Head, typename... Tail>
struct max_alignof<Head, Tail...>
    : std::integral_constant<std::size_t,
          (alignof(Head) > max_alignof<Tail...>::value) ? alignof(Head) : max_alignof<Tail...>::value> {};

template<typename T, typename... Types>
struct type_index;

template<typename T, typename Head, typename... Tail>
struct type_index<T, Head, Tail...>
    : std::integral_constant<std::size_t, std::is_same_v<T, Head> ? 0 : 1 + type_index<T, Tail...>::value> {};

template<typename T>
struct type_index<T> : std::integral_constant<std::size_t, 0> {};

template<std::size_t N, typename... Types>
struct nth_pack_type;

template<std::size_t N, typename Head, typename... Tail>
struct nth_pack_type<N, Head, Tail...> : nth_pack_type<N - 1, Tail...> {};

template<typename Head, typename... Tail>
struct nth_pack_type<0, Head, Tail...> { using type = Head; };

template<typename T, typename... Types>
concept variant_one_of = (std::is_same_v<T, Types> || ...);

} // namespace detail

/// Trivially-copyable variant. All alternative types must be trivially copyable.
template<trivially_copyable... Types>
    requires (sizeof...(Types) > 0) && (sizeof...(Types) <= 255)
struct variant {
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    template<std::size_t I>
        requires (I < sizeof...(Types))
    using type_at = typename detail::nth_pack_type<I, Types...>::type;

private:
    alignas(detail::max_alignof<Types...>::value)
    char _storage[detail::max_sizeof<Types...>::value]{};
    std::uint8_t _index{static_cast<std::uint8_t>(npos)};

    template<std::size_t I, typename T>
    constexpr void construct_at(const T& value) noexcept {
        __builtin_memcpy(_storage, &value, sizeof(T));
        _index = static_cast<std::uint8_t>(I);
    }

public:
    variant()                          = default;
    variant(const variant&)            = default;
    variant& operator=(const variant&) = default;
    variant(variant&&)                 = default;
    variant& operator=(variant&&)      = default;
    ~variant()                         = default;

    template<typename T>
        requires detail::variant_one_of<T, Types...>
    constexpr variant(const T& value) noexcept {
        constexpr std::size_t idx = detail::type_index<T, Types...>::value;
        construct_at<idx>(value);
    }

    template<typename T>
        requires detail::variant_one_of<T, Types...>
    constexpr variant& operator=(const T& value) noexcept {
        constexpr std::size_t idx = detail::type_index<T, Types...>::value;
        construct_at<idx>(value);
        return *this;
    }

    [[nodiscard]] constexpr std::size_t index() const noexcept {
        return _index == static_cast<std::uint8_t>(npos) ? npos : static_cast<std::size_t>(_index);
    }

    [[nodiscard]] constexpr bool valueless_by_exception() const noexcept {
        return _index == static_cast<std::uint8_t>(npos);
    }

    template<typename T>
        requires detail::variant_one_of<T, Types...>
    [[nodiscard]] constexpr bool holds_alternative() const noexcept {
        return index() == detail::type_index<T, Types...>::value;
    }

    template<std::size_t I>
        requires (I < sizeof...(Types))
    [[nodiscard]] constexpr auto& get() noexcept {
        using T = typename detail::nth_pack_type<I, Types...>::type;
        return *reinterpret_cast<T*>(_storage);
    }

    template<std::size_t I>
        requires (I < sizeof...(Types))
    [[nodiscard]] constexpr const auto& get() const noexcept {
        using T = typename detail::nth_pack_type<I, Types...>::type;
        return *reinterpret_cast<const T*>(_storage);
    }

    template<typename T>
        requires detail::variant_one_of<T, Types...>
    [[nodiscard]] constexpr T* get_if() noexcept {
        constexpr std::size_t idx = detail::type_index<T, Types...>::value;
        return (index() == idx) ? reinterpret_cast<T*>(_storage) : nullptr;
    }

    template<typename T>
        requires detail::variant_one_of<T, Types...>
    [[nodiscard]] constexpr const T* get_if() const noexcept {
        constexpr std::size_t idx = detail::type_index<T, Types...>::value;
        return (index() == idx) ? reinterpret_cast<const T*>(_storage) : nullptr;
    }

    template<typename Visitor>
    constexpr decltype(auto) visit(Visitor&& visitor) const {
        return visit_impl<0>(std::forward<Visitor>(visitor));
    }

    template<typename Visitor>
    constexpr decltype(auto) visit(Visitor&& visitor) {
        return visit_impl<0>(std::forward<Visitor>(visitor));
    }

    [[nodiscard]] constexpr bool operator==(const variant& other) const noexcept {
        if (_index != other._index) return false;
        if (valueless_by_exception()) return true;
        return __builtin_memcmp(_storage, other._storage, detail::max_sizeof<Types...>::value) == 0;
    }

    [[nodiscard]] constexpr bool operator!=(const variant& other) const noexcept {
        return !(*this == other);
    }

private:
    template<std::size_t I, typename Visitor>
    constexpr decltype(auto) visit_impl(Visitor&& visitor) const {
        if constexpr (I >= sizeof...(Types)) {
            __builtin_unreachable();
        } else {
            if (index() == I) return std::forward<Visitor>(visitor)(get<I>());
            if constexpr (I + 1 < sizeof...(Types)) return visit_impl<I + 1>(std::forward<Visitor>(visitor));
            else __builtin_unreachable();
        }
    }

    template<std::size_t I, typename Visitor>
    constexpr decltype(auto) visit_impl(Visitor&& visitor) {
        if constexpr (I >= sizeof...(Types)) {
            __builtin_unreachable();
        } else {
            if (index() == I) return std::forward<Visitor>(visitor)(get<I>());
            if constexpr (I + 1 < sizeof...(Types)) return visit_impl<I + 1>(std::forward<Visitor>(visitor));
            else __builtin_unreachable();
        }
    }
};

static_assert(std::is_trivially_copyable_v<variant<int, float, char>>,
              "embed::variant must be trivially copyable");

} // namespace embed

// ── Structured bindings support ──────────────────────────────────────────

template<embed::trivially_copyable First, embed::trivially_copyable Second>
struct std::tuple_size<embed::pair<First, Second>>
    : std::integral_constant<std::size_t, 2> {};

template<std::size_t Index, embed::trivially_copyable First, embed::trivially_copyable Second>
struct std::tuple_element<Index, embed::pair<First, Second>> {
    using type = std::conditional_t<Index == 0, First, Second>;
};

template<std::size_t Index, embed::trivially_copyable First, embed::trivially_copyable Second>
constexpr auto& get(embed::pair<First, Second>& p) noexcept { return p.template get<Index>(); }

template<std::size_t Index, embed::trivially_copyable First, embed::trivially_copyable Second>
constexpr const auto& get(const embed::pair<First, Second>& p) noexcept { return p.template get<Index>(); }
