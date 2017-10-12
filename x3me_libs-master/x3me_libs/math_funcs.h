#pragma once
// The name of this file can't be math.h because it collide with
// the C-library math.h and bad things happen during the build

#include <type_traits>
#include "mpl.h"

namespace x3me
{
namespace math
{

template <typename T, typename U>
constexpr auto divide_round_up(const T& x, const U& y) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
    static_assert(std::is_integral<U>::value || std::is_enum<U>::value, "");
    return ((x + y - 1) / y);
}

// Rounds up to the multiple
template <typename T, typename U>
constexpr auto round_up(const T& num, const U& multiple) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
    static_assert(std::is_integral<U>::value || std::is_enum<U>::value, "");
    return ((num + multiple - 1) / multiple) * multiple;
}

// Rounds up to the multiple which must be a power of two
template <typename T, typename U>
constexpr auto round_up_pow2(const T& num, const U& multiple) noexcept
{
    static_assert(std::is_unsigned<T>::value || mpl::is_unsigned_enum<T>::value,
                  "");
    static_assert(std::is_unsigned<U>::value || mpl::is_unsigned_enum<U>::value,
                  "");
    return ((num + multiple - 1) & ~(multiple - 1));
}

// Range [x1, x2)
template <typename T, typename U>
constexpr bool in_range(const T& v, const U& x1, const U& x2) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
    static_assert(std::is_integral<U>::value || std::is_enum<U>::value, "");
    return (x1 <= v) && (v < x2);
}

// Ranges [x1, x2) and [y1, y2)
// Returns true if the first range is inside the second one.
template <typename T, typename U>
constexpr bool in_range(const T& x1, const T& x2, const U& y1,
                        const U& y2) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
    static_assert(std::is_integral<U>::value || std::is_enum<U>::value, "");
    return (y1 <= x1) && (x2 <= y2);
}

// Ranges [x1, x2) and [y1, y2)
template <typename T, typename U>
constexpr std::common_type_t<T, U>
ranges_overlap(const T& x1, const T& x2, const U& y1, const U& y2) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
    static_assert(std::is_integral<U>::value || std::is_enum<U>::value, "");
    const auto mx = x1 > y1 ? x1 : y1;
    const auto mn = x2 < y2 ? x2 : y2;
    return mn > mx ? mn - mx : 0;
}

// Returns if an integer number is power of two.
// Returns true of 0 which may or may not be wanted.
template <typename T>
constexpr bool is_pow_of_2(const T& n)
{
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
    return ((n & (n - 1)) == 0);
}

} // namespace math
} // namespace x3me
