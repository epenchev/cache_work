#pragma once

constexpr uint64_t operator"" _KB(unsigned long long int v)
{
    return v * 1024;
}
constexpr uint64_t operator"" _MB(unsigned long long int v)
{
    return v * 1024 * 1024;
}
constexpr uint64_t operator"" _GB(unsigned long long int v)
{
    return v * 1024 * 1024 * 1024;
}
constexpr uint64_t operator"" _TB(unsigned long long int v)
{
    return v * 1024 * 1024 * 1024 * 1024;
}

////////////////////////////////////////////////////////////////////////////////

constexpr uint64_t operator"" _KB(long double v)
{
    return v * 1024;
}
constexpr uint64_t operator"" _MB(long double v)
{
    return v * 1024 * 1024;
}
constexpr uint64_t operator"" _GB(long double v)
{
    return v * 1024 * 1024 * 1024;
}
constexpr uint64_t operator"" _TB(long double v)
{
    return v * 1024 * 1024 * 1024 * 1024;
}

////////////////////////////////////////////////////////////////////////////////

inline string_view_t to_string_view(const boost_string_t& s) noexcept
{
    return {s.data(), s.size()};
}
