#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x3me_assert.h"

namespace x3me
{
namespace assert
{
namespace detail
{

void fail_abort(const char* file_line, const char* fun, const char* expr,
                const char* msg) noexcept
{
    const char assert_failed[] = ". Assert failed: ";
    // Don't use functions which may cause buffering in the user space
    // because the program state is 'kind of' unstable in the point of
    // assertion. It's inefficient because of the lots of the kernel calls
    // but we are going to abort the program anyway.
    auto r = write(STDERR_FILENO, file_line, strlen(file_line));
    r      = write(STDERR_FILENO, ", ", 2);
    r      = write(STDERR_FILENO, fun, strlen(fun));
    r      = write(STDERR_FILENO, assert_failed, sizeof(assert_failed) - 1);
    r = write(STDERR_FILENO, expr, strlen(expr));
    if (msg)
    {
        r = write(STDERR_FILENO, ". ", 2);
        r = write(STDERR_FILENO, msg, strlen(msg));
    }
    r = write(STDERR_FILENO, ".\n", 2);

    (void)r; // Silence warning for unused result

    abort();
}

void fail_throw(const char* file_line, const char* fun, const char* expr,
                const char* msg)
{
    // Don't throw if there is an exception in flight because this
    // could terminate the program from a different place.
    if (!std::uncaught_exception())
        throw assert_fail(file_line, fun, expr, msg);
    else
        fail_abort(file_line, fun, expr, msg);
}

} // namespace detail
} // namespace assert
} // namespace x3me
