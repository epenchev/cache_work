#pragma once

namespace x3me
{
namespace str_utils
{

class const_string
{
    const char* const p_;
    const size_t s_;

public:
    template <size_t N>
    constexpr const_string(const char(&a)[N])
        : p_(a), s_(N - 1)
    {
        static_assert(N >= 1, "Must be a string literal");
    }
    constexpr auto c_str() const { return p_; }
    constexpr auto data() const { return p_; }
    constexpr auto size() const { return s_; }
    constexpr auto begin() const { return p_; }
    constexpr auto end() const { return p_ + s_; }
};

} // namespace str_utils
} // namespace x3me
