#pragma once

#include <type_traits>

namespace x3me
{
namespace phint
{
namespace detail
{
template <typename T>
struct can_load_register
{
    using decayed_t = std::decay_t<T>;

    // We can put something in a register if:
    // 1. The thing is bigger than the machine word, AND
    // 2. It's a pointer to something. We want 'the something' to be read/put
    // from/to the memory not the pointer to it, AND
    // 3. It's not a trivially copyable type. The compiler won't put such types
    // into a register.
    constexpr static bool value = (sizeof(decayed_t) <= sizeof(void*)) &&
                                  !std::is_pointer<decayed_t>::value &&
                                  std::is_trivially_copyable<decayed_t>::value;
};
} // namespace detail

template <typename T>
auto do_not_optimize_away(const T& v) noexcept
    -> std::enable_if_t<detail::can_load_register<T>::value>
{
    asm volatile("" ::"r"(v)); // We are allowed to put it into a register
}

template <typename T>
auto do_not_optimize_away(const T& v) noexcept
    -> std::enable_if_t<!detail::can_load_register<T>::value>
{
    // We are not allowed to load the value to a register thus we tell the
    // compiler to load it from the memory.
    // The input comes from the memory '"m"(v)' and the 'memory' clobber
    // ensures that the compiler will flush the needed things from 'v'
    // to the memory before the assembly here tries to read them.
    asm volatile("" ::"m"(v) : "memory");
}

} // namespace phint
} // namespace x3me

#define X3ME_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define X3ME_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define X3ME_EXPECT(expr, value) __builtin_expect((expr), (value))

// This fence forces the compiler to flush all values, computed before it,
// to the memory, and read all values, needed after it, from the memory.
#define X3ME_OPTIMIZATION_FENCE asm volatile("" ::: "memory")

// Prevents the compiler from removing computations of the given value.
// The compilers are allowed to do that if they can prove that the value is
// not actually used (for IO read/write, or smth like that).
// The macro is useful for micro benchmarks where the compiler can remove the
// whole benchmarked function if it's produced value is not used.
#define X3ME_DO_NOT_OPTIMIZE_AWAY(v) x3me::phint::do_not_optimize_away(v)
