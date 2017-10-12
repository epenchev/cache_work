#pragma once

#include <array>
#include <cstring>
#include <ostream>
#include <string>

namespace x3me
{
namespace str_utils
{

// TODO This class needs to be made fully functional
// NOTE that the size includes the zero terminator
template <size_t Size>
class stack_string : protected std::array<char, Size>
{
    using parent_t = std::array<char, Size>;

public:
    enum
    {
        scapacity = Size - 1
    };

public:
    stack_string() noexcept { parent_t::operator[](0) = '\0'; }

    // If the string is longer than the capacity it'll truncate it silently
    explicit stack_string(const std::string& s) noexcept { assign(s.c_str()); }
    // If the string is longer than the capacity it'll truncate it silently
    explicit stack_string(const char* p) noexcept { assign(p); }
    stack_string(const char* p, size_t len) noexcept { assign(p, len); }

    // If the string is longer than the capacity it'll truncate it silently
    void assign(const char* p) noexcept
    {
        char* dst = parent_t::data();
        std::strncpy(dst, p, scapacity);
        dst[scapacity] = '\0';
    }

    // If the string is longer than the capacity it'll truncate it silently
    void assign(const char* p, size_t len) noexcept
    {
        char* dst = parent_t::data();
        len = (len < size_t(scapacity)) ? len : size_t(scapacity);
        std::strncpy(dst, p, len);
        dst[len] = '\0';
    }

    const char* data() const noexcept { return parent_t::data(); }
    const char* c_str() const noexcept { return data(); }

    // The strings will be short in 99.99% cases.
    // That's why I prefer linear size instead of additional member variable
    // which would lead to an increased size
    size_t size() const noexcept { return std::strlen(data()); }
    constexpr size_t capacity() const noexcept { return scapacity; }

    friend bool operator<(const stack_string& lhs, const stack_string& rhs)
    {
        return (std::strcmp(lhs.c_str(), rhs.c_str()) < 0);
    }
    friend bool operator==(const stack_string& lhs, const stack_string& rhs)
    {
        return (std::strcmp(lhs.c_str(), rhs.c_str()) == 0);
    }
    friend bool operator!=(const stack_string& lhs, const stack_string& rhs)
    {
        return !(lhs == rhs);
    }
};

template <size_t Size>
std::ostream& operator<<(std::ostream& os, const stack_string<Size>& rhs)
{
    os.write(rhs.data(), rhs.size());
    return os;
}

} // namespace str_utils
} // namespace x3me
