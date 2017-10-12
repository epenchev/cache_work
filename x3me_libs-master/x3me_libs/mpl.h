#pragma once

#include <limits>
#include <type_traits>

namespace x3me
{
namespace mpl
{
namespace detail
{

struct empty
{
};

} // namespace detail

////////////////////////////////////////////////////////////////////////////////

template <typename T>
constexpr uint64_t num_digits(T n)
{
    static_assert(std::is_integral<T>::value, "Integer required");
    return (n != 0) ? num_digits(n / 10) + 1 : 0;
}

template <typename T>
constexpr uint64_t max_num_digits()
{
    return num_digits(std::numeric_limits<T>::max());
}

////////////////////////////////////////////////////////////////////////////////

template <bool Condition, int64_t...>
struct num_check
{
    enum
    {
        value = Condition
    };
    static_assert(Condition, "");
};

template <bool Condition, typename...>
struct type_check
{
    enum
    {
        value = Condition
    };
    static_assert(Condition, "");
};

// Better use simple static_assert if you don't need
// additional arguments to be printed in case of compiler error
#define X3ME_STATIC_NUM_CHECK(cond, ...)                                       \
    static_assert(x3me::mpl::num_check<(cond), __VA_ARGS__>::value, "")

#define X3ME_STATIC_TYPE_CHECK(cond, ...)                                      \
    static_assert(x3me::mpl::type_check<(cond), __VA_ARGS__>::value, "")

////////////////////////////////////////////////////////////////////////////////

template <typename Arg0 = detail::empty, typename... Args>
class typelist
{
    using typelist_t               = typelist<Args...>;
    using internal_max_size_type_t = typename typelist_t::max_size_type_t;

public:
    using max_size_type_t =
        typename std::conditional<sizeof(Arg0) >=
                                      sizeof(internal_max_size_type_t),
                                  Arg0, internal_max_size_type_t>::type;
    enum
    {
        length = 1 + typelist_t::length,
    };
};

template <>
class typelist<detail::empty>
{
public:
    using max_size_type_t = uint8_t;
    enum
    {
        length = 0,
    };
};

////////////////////////////////////////////////////////////////////////////////

// TODO Make them like the remaining std traits deriving from
// std::integral_constant. No time for this currently
template <typename T>
class is_signed_enum
{
    static constexpr bool is(std::true_type)
    {
        return std::is_signed<typename std::underlying_type<T>::type>::value;
    }
    static constexpr bool is(std::false_type) { return false; }
public:
    enum
    {
        value = is(typename std::is_enum<T>::type{})
    };
};

template <typename T>
class is_unsigned_enum
{
    static constexpr bool is(std::true_type)
    {
        return std::is_unsigned<typename std::underlying_type<T>::type>::value;
    }
    static constexpr bool is(std::false_type) { return false; }
public:
    enum
    {
        value = is(typename std::is_enum<T>::type{})
    };
};

////////////////////////////////////////////////////////////////////////////////
// This functionality is mostly for internal usage in the x3me_libs
#define X3ME_DEFINE_HAS_METHOD(method)                                         \
    template <typename, typename>                                              \
    class has_##method;                                                        \
    template <typename C, typename R, typename... A>                           \
    class has_##method<C, R(A...)>                                             \
    {                                                                          \
        template <typename T>                                                  \
        static auto check(T*) -> typename std::is_same<                        \
            decltype(std::declval<T>().method(std::declval<A>()...)),          \
            R>::type;                                                          \
        template <typename>                                                    \
        static std::false_type check(...);                                     \
                                                                               \
    public:                                                                    \
        static constexpr bool value = decltype(check<C>(nullptr))::value;      \
    }

#define X3ME_HAS_METHOD(class_name, method, signature)                         \
    has_##method<class_name, signature>::value
#define X3ME_HAS_METHOD_NMSP(nmsp, class_name, method, signature)              \
    nmsp::has_##method<class_name, signature>::value

} // namespace mpl
} // namespace x3me
