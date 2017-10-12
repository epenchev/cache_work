#pragma once

#include <endian.h>

#include <array>
#include <cstdio>
#include <cstring>

#include "mpl.h"

namespace x3me
{
namespace convert
{
namespace detail
{

template <typename T, size_t BufferSize>
constexpr size_t size(const std::array<T, BufferSize>&) noexcept
{
    return BufferSize;
}
template <typename T, size_t BufferSize>
constexpr size_t size(const T(&)[BufferSize]) noexcept
{
    return BufferSize;
}
template <typename T, size_t BufferSize>
constexpr const T* data(const std::array<T, BufferSize>& buff) noexcept
{
    return &buff[0];
}
template <typename T, size_t BufferSize>
constexpr const T* data(const char(&buff)[BufferSize]) noexcept
{
    return buff;
}
template <typename T, size_t BufferSize>
constexpr T* data(std::array<T, BufferSize>& buff) noexcept
{
    return &buff[0];
}
template <typename T, size_t BufferSize>
constexpr T* data(T(&buff)[BufferSize]) noexcept
{
    return buff;
}

// Returns number of characters printed (excluding the null terminator)
// or 0 in case of error (in practice shouldn't happen)
template <typename Num, typename Buff>
inline uint32_t int_to_str_l(const Num& num, const char* format,
                             Buff& buff) noexcept
{
    enum
    {
        min_size = std::is_signed<Num>::value + // the sign
                   mpl::max_num_digits<Num>() + // the digits
                   1 // the null terminator
    };
    static_assert(size(Buff{}) >= min_size, "");
    int r = std::snprintf(data(buff), size(buff), format, num);
    assert(r > 0 && r < min_size);
    return (r > 0) ? r : 0;
}

// Returns a pointer to the buffer or null in case of failure.
// However, it should never return null in practice
template <typename Num, typename Buff>
inline auto int_to_str_s(const Num& num, const char* format,
                         Buff& buff) noexcept -> decltype(data(buff))
{
    enum
    {
        min_size = std::is_signed<Num>::value + // the sign
                   mpl::max_num_digits<Num>() + // the digits
                   1 // the null terminator
    };
    static_assert(size(Buff{}) >= min_size, "");
    int r = std::snprintf(data(buff), size(buff), format, num);
    assert(r > 0 && r < min_size);
    return (r > 0) ? data(buff) : nullptr;
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////

#define CONT_NUM_TYPES(MACRO)                                                  \
    MACRO(int8_t, "%hhi")                                                      \
    MACRO(uint8_t, "%hhu")                                                     \
    MACRO(int16_t, "%hi")                                                      \
    MACRO(uint16_t, "%hu")                                                     \
    MACRO(int32_t, "%i")                                                       \
    MACRO(uint32_t, "%u")                                                      \
    MACRO(int64_t, "%lli")                                                     \
    MACRO(uint64_t, "%llu")

#define IT_FUNC_INT_TO_STR(type, format)                                       \
    template <typename Buff>                                                   \
    inline auto int_to_str_l(const type& num, Buff& buff) noexcept             \
    {                                                                          \
        return detail::int_to_str_l(num, format, buff);                        \
    }                                                                          \
    template <typename Buff>                                                   \
    inline auto int_to_str_s(const type& num, Buff& buff) noexcept             \
    {                                                                          \
        return detail::int_to_str_s(num, format, buff);                        \
    }

CONT_NUM_TYPES(IT_FUNC_INT_TO_STR)

#undef IT_FUNC_INT_TO_STR
#undef CONT_NUM_TYPES

////////////////////////////////////////////////////////////////////////////////

#define CONT_FUNC_INFO(MACRO)                                                  \
    MACRO(htobe16, be16toh, 2)                                                 \
    MACRO(htobe32, be32toh, 4)                                                 \
    MACRO(htobe64, be64toh, 8)

#define IT_FUNC_HTOBE(func_htobe, _, size)                                     \
    template <typename T>                                                      \
    inline T func_htobe##_num(T n) noexcept                                    \
    {                                                                          \
        static_assert(sizeof(T) == size, "");                                  \
        return func_htobe(n);                                                  \
    }
#define IT_FUNC_BETOH(_, func_betoh, size)                                     \
    template <typename T>                                                      \
    inline T func_betoh##_num(T n) noexcept                                    \
    {                                                                          \
        static_assert(sizeof(T) == size, "");                                  \
        return func_betoh(n);                                                  \
    }

CONT_FUNC_INFO(IT_FUNC_HTOBE)
CONT_FUNC_INFO(IT_FUNC_BETOH)

#undef IT_FUNC_HTOBE
#undef IT_FUNC_BETOH

////////////////////////////////////////////////////////////////////////////////

#define IT_FUNC_WRITE_HTOBE(func_htobe, _u1, _u2)                              \
    template <typename T, typename Buff>                                       \
    inline auto write_##func_htobe(T n, Buff& b) noexcept                      \
    {                                                                          \
        static_assert(sizeof(T) <= detail::size(Buff{}), "");                  \
        auto v = func_htobe##_num(n);                                          \
        std::memcpy(detail::data(b), &v, sizeof(v));                           \
        return detail::data(b);                                                \
    }
#define IT_FUNC_WRITE_HTOBE_UNSAFE(func_htobe, _u1, _u2)                       \
    template <typename T, typename Buff>                                       \
    inline auto write_##func_htobe##_unsafe(T n, Buff* b) noexcept             \
    {                                                                          \
        auto v = func_htobe##_num(n);                                          \
        std::memcpy(b, &v, sizeof(v));                                         \
        return b;                                                              \
    }
#define IT_FUNC_READ_BETOH_UNSAFE(_u1, func_betoh, _u2)                        \
    template <typename T, typename Buff>                                       \
    inline T read_##func_betoh##_unsafe(const Buff* b) noexcept                \
    {                                                                          \
        T n;                                                                   \
        std::memcpy(&n, b, sizeof(T));                                         \
        return func_betoh##_num(n);                                            \
    }

CONT_FUNC_INFO(IT_FUNC_WRITE_HTOBE)
CONT_FUNC_INFO(IT_FUNC_WRITE_HTOBE_UNSAFE)
CONT_FUNC_INFO(IT_FUNC_READ_BETOH_UNSAFE)

#undef IT_FUNC_WRITE_HTOBE
#undef IT_FUNC_WRITE_HTOBE_UNSAFE
#undef IT_FUNC_READ_BETOH_UNSAFE

#undef CONT_FUNC_INFO

} // namespace convert
} // namespace x3me
