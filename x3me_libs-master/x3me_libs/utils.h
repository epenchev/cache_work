#pragma once

#include <stdlib.h>

namespace x3me
{
namespace utils
{

template <typename VectorType, typename... Args>
void resize_vector(VectorType& v, size_t size, const Args&... args)
{
    v.reserve(size);
    for (size_t i = v.size(); i < size; ++i)
    {
        v.emplace_back(args...);
    }
}

////////////////////////////////////////////////////////////////////////////////

template <typename Cont>
constexpr auto data(Cont& cont) noexcept
{
    return cont.data();
}

template <typename T, size_t BufferSize>
constexpr auto data(T(&buff)[BufferSize]) noexcept
{
    return buff;
}

template <typename Cont>
constexpr auto size(const Cont& c) noexcept
{
    return c.size();
}

template <typename T, size_t Size>
constexpr auto size(const T(&)[Size]) noexcept
{
    return Size;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
constexpr auto min(const T& v)
{
    return v;
}

template <typename Arg, typename... Args>
constexpr auto min(const Arg& arg, const Args&... args)
{
    return arg < min(args...) ? arg : min(args...);
}

template <typename T>
constexpr auto max(const T& v)
{
    return v;
}

template <typename Arg, typename... Args>
constexpr auto max(const Arg& arg, const Args&... args)
{
    return arg > max(args...) ? arg : max(args...);
}

} // namespace utils
} // namespace x3me
