#pragma once

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <type_traits>

namespace embed {

/// Fixed-size, trivially-copyable string.
/// Suitable for use in messages passed through esp_event_loop.
///
/// @tparam Capacity Maximum number of characters (excluding null-terminator).
///                   Internal buffer size is Capacity + 1.
template<std::size_t Capacity>
struct string {
    char _data[Capacity + 1]{};

    string()                         = default;
    string(const string&)            = default;
    string& operator=(const string&) = default;
    string(string&&)                 = default;
    string& operator=(string&&)      = default;
    ~string()                        = default;

    /// Construct from a null-terminated C string.
    constexpr explicit string(const char* source) {
        if (source) {
            const std::size_t length = std::min(Capacity, std::strlen(source));
            std::copy_n(source, length, _data);
            _data[length] = '\0';
        }
    }

    /// Construct from a pointer and length.
    constexpr string(const char* source, std::size_t length) {
        if (source) {
            length = std::min(Capacity, length);
            std::copy_n(source, length, _data);
            _data[length] = '\0';
        }
    }

    /// Construct from any string-like object (has c_str() and size()).
    template<typename StringLike>
        requires requires(const StringLike& s) {
            { s.c_str() } -> std::convertible_to<const char*>;
            { s.size()  } -> std::convertible_to<std::size_t>;
        }
    constexpr explicit string(const StringLike& source) {
        const std::size_t length = std::min(Capacity, static_cast<std::size_t>(source.size()));
        std::copy_n(source.c_str(), length, _data);
        _data[length] = '\0';
    }

    // ── Assignment ────────────────────────────────────────────────────────

    constexpr string& operator=(const char* source) {
        if (source) {
            const std::size_t length = std::min(Capacity, std::strlen(source));
            std::copy_n(source, length, _data);
            _data[length] = '\0';
        } else {
            _data[0] = '\0';
        }
        return *this;
    }

    // ── Cross-size copy ───────────────────────────────────────────────────

    template<std::size_t OtherCapacity>
    constexpr explicit string(const string<OtherCapacity>& other) {
        const std::size_t length = std::min(Capacity, other.size());
        std::copy_n(other.c_str(), length, _data);
        _data[length] = '\0';
    }

    // ── Accessors ─────────────────────────────────────────────────────────

    [[nodiscard]] constexpr const char* c_str()    const noexcept { return _data; }
    [[nodiscard]] constexpr const char* data()     const noexcept { return _data; }
    [[nodiscard]] constexpr char*       data()           noexcept { return _data; }
    [[nodiscard]] constexpr std::size_t size()     const noexcept { return std::strlen(_data); }
    [[nodiscard]] constexpr std::size_t length()   const noexcept { return size(); }
    [[nodiscard]] constexpr bool        empty()    const noexcept { return _data[0] == '\0'; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    constexpr char  operator[](std::size_t index) const noexcept { return _data[index]; }
    constexpr char& operator[](std::size_t index)       noexcept { return _data[index]; }

    // ── Iterators ─────────────────────────────────────────────────────────

    [[nodiscard]] constexpr const char* begin()  const noexcept { return _data; }
    [[nodiscard]] constexpr const char* end()    const noexcept { return _data + size(); }
    [[nodiscard]] constexpr char*       begin()        noexcept { return _data; }
    [[nodiscard]] constexpr char*       end()          noexcept { return _data + size(); }

    // ── Search ────────────────────────────────────────────────────────────

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    [[nodiscard]] constexpr std::size_t find(char character, std::size_t startPos = 0) const noexcept {
        const std::size_t len = size();
        for (std::size_t i = startPos; i < len; ++i) {
            if (_data[i] == character) return i;
        }
        return npos;
    }

    [[nodiscard]] constexpr bool contains(char character) const noexcept {
        return find(character) != npos;
    }

    [[nodiscard]] constexpr bool starts_with(const char* prefix) const noexcept {
        if (!prefix) return false;
        const std::size_t prefixLen = std::strlen(prefix);
        return size() >= prefixLen && std::strncmp(_data, prefix, prefixLen) == 0;
    }

    // ── Modifiers ─────────────────────────────────────────────────────────

    constexpr void clear() noexcept { _data[0] = '\0'; }

    /// Assign from pointer and length (like std::string::assign).
    constexpr string& assign(const char* source, std::size_t length) {
        if (source) {
            length = std::min(Capacity, length);
            std::copy_n(source, length, _data);
            _data[length] = '\0';
        } else {
            _data[0] = '\0';
        }
        return *this;
    }

    constexpr string& append(const char* source) {
        if (source) {
            const std::size_t currentLength = size();
            const std::size_t remaining = Capacity - currentLength;
            const std::size_t appendLength = std::min(remaining, std::strlen(source));
            std::copy_n(source, appendLength, _data + currentLength);
            _data[currentLength + appendLength] = '\0';
        }
        return *this;
    }

    constexpr string& operator+=(const char* source) { return append(source); }

    // ── Comparison ────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(const string& other) const noexcept {
        return std::strcmp(_data, other._data) == 0;
    }

    [[nodiscard]] constexpr bool operator==(const char* other) const noexcept {
        return other && std::strcmp(_data, other) == 0;
    }

    [[nodiscard]] constexpr auto operator<=>(const string& other) const noexcept {
        return std::strcmp(_data, other._data) <=> 0;
    }

    [[nodiscard]] constexpr explicit operator const char*() const noexcept { return _data; }
};

static_assert(std::is_trivially_copyable_v<string<1>>,
              "embed::string must be trivially copyable");
static_assert(std::is_trivially_copyable_v<string<64>>,
              "embed::string must be trivially copyable");

} // namespace embed
