#pragma once

#include <exception>

#include "perf_hint.h"

namespace x3me
{
namespace assert
{

class assert_fail final : public std::exception
{
    const char* const file_line_; // not nullptr
    const char* const func_name_; // not nullptr
    const char* const expr_; // not nullptr
    const char* const msg_; // can be nullptr

public:
    assert_fail(const char* file_line, const char* fun, const char* expr,
                const char* msg) noexcept : file_line_(file_line),
                                            func_name_(fun),
                                            expr_(expr),
                                            msg_(msg)
    {
    }

    assert_fail() = delete;
    ~assert_fail() noexcept final = default;
    assert_fail(const assert_fail&) noexcept = default;
    assert_fail& operator=(const assert_fail&) noexcept = default;
    assert_fail(assert_fail&&) noexcept = default;
    assert_fail& operator=(assert_fail&&) noexcept = default;

    const char* what() const noexcept final { return "Assert failed"; }

    const char* file_line() const noexcept { return file_line_; }
    const char* func_name() const noexcept { return func_name_; }
    const char* expression() const noexcept { return expr_; }
    const char* message() const noexcept { return msg_; }
};

////////////////////////////////////////////////////////////////////////////////
namespace detail
{

[[noreturn]] void fail_abort(const char* file_line, const char* fun,
                             const char* expr, const char* msg) noexcept;
// The following function throws assert_fail exception if there is no
// exception already in flight. Otherwise it calls fail_abort.
[[noreturn]] void fail_throw(const char* file_line, const char* fun,
                             const char* expr, const char* msg);

} // namespace detail
} // namespace assert
} // namespace x3me
////////////////////////////////////////////////////////////////////////////////
// Locally used 'private' macros

#define X3ME_STRINGIZE_STEP2(v) #v
#define X3ME_STRINGIZE(v) X3ME_STRINGIZE_STEP2(v)

#define X3ME_ASSERT_NO_MSG(fail_fun, expr)                                     \
    do                                                                         \
    {                                                                          \
        if (X3ME_UNLIKELY(!(expr)))                                            \
            fail_fun(__FILE__ ":" X3ME_STRINGIZE(__LINE__),                    \
                     __PRETTY_FUNCTION__, #expr, nullptr);                     \
    } while (0)

// The compilation will fail here if the msg is not a literal string
#define X3ME_ASSERT_MSG(fail_fun, expr, msg)                                   \
    do                                                                         \
    {                                                                          \
        if (X3ME_UNLIKELY(!(expr)))                                            \
        {                                                                      \
            constexpr const char* m = msg;                                     \
            fail_fun(__FILE__ ":" X3ME_STRINGIZE(__LINE__),                    \
                     __PRETTY_FUNCTION__, #expr, m);                           \
        }                                                                      \
    } while (0)

#define X3ME_OVERLOAD_MACRO(_1, _2, NAME, ...) NAME

////////////////////////////////////////////////////////////////////////////////
// The X3ME_ENFORCE is always active.
// Example usage:
//
// X3ME_ENFORCE(x == 42);
//
// or
//
// X3ME_ENFORCE(x == 42, "x must be 42");
//
// Note that for the second usage the description message must be string literal
// otherwise the compilation will fail.
#ifdef X3ME_ASSERT_DO_THROW

#define X3ME_ENFORCE(...)                                                      \
    X3ME_OVERLOAD_MACRO(__VA_ARGS__, X3ME_ASSERT_MSG, X3ME_ASSERT_NO_MSG)      \
    (x3me::assert::detail::fail_throw, __VA_ARGS__)

#define NOEXCEPT_ON_X3ME_ENFORCE

#else // Abort by default

#define X3ME_ENFORCE(...)                                                      \
    X3ME_OVERLOAD_MACRO(__VA_ARGS__, X3ME_ASSERT_MSG, X3ME_ASSERT_NO_MSG)      \
    (x3me::assert::detail::fail_abort, __VA_ARGS__)

#define NOEXCEPT_ON_X3ME_ENFORCE noexcept

#endif // X3ME_ASSERT_THROW

////////////////////////////////////////////////////////////////////////////////
// The X3ME_ASSERT is active only when NDEBUG is not present
// Example usage:
//
// X3ME_ASSERT(x == 42);
//
// or
//
// X3ME_ASSERT(x == 42, "x must be 42");
//
// Note that for the second usage the description message must be string literal
// otherwise the compilation will fail.
#ifndef NDEBUG

#define X3ME_ASSERT(...) X3ME_ENFORCE(__VA_ARGS__)
#define NOEXCEPT_ON_X3ME_ASSERT NOEXCEPT_ON_X3ME_ENFORCE

#else

// Functions to silence the compiler warnings for unused parameters.
// Note that the trick '(void)expr' will silence the compiler, but the
// 'expression' will still be evaluated which is unwanted.
#define X3ME_SKIP_EVAL(e) static_cast<void>(sizeof(e))
#define X3ME_SKIP_EVAL2(_1, _2) X3ME_SKIP_EVAL(_1), X3ME_SKIP_EVAL(_2)

#define X3ME_ASSERT(...)                                                       \
    X3ME_OVERLOAD_MACRO(__VA_ARGS__, X3ME_SKIP_EVAL2, X3ME_SKIP_EVAL)          \
    (__VA_ARGS__)

#define NOEXCEPT_ON_X3ME_ASSERT noexcept

#endif // NDEBUG
