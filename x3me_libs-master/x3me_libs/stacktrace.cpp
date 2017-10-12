#include <sys/types.h>
#include <sys/uio.h>

#include <execinfo.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stacktrace.h"
#include "x3me_assert.h"

namespace x3me
{
namespace sys_utils
{
namespace detail
{

struct demangled_info
{
    iovec data[4];
    int size;
};

// The mangled info is in the following format:
// <path_to_module>(<mangled_function_name>+<hex_offset>) [<hex_address>]
// The demangled info is in the following format:
// [0] = <path_to_module>(
// [1] = <demangled_function_name>
// [2] = +<hex_offset>) [<hex_address>]
void demangle(char *mangled, char *&buff, size_t len, demangled_info &demangled)
{
    char *found1 = nullptr;
    char *found2 = nullptr;

    found1 = strchr(mangled, '(');
    if (found1)
    {
        ++found1;
        found2 = strchr(found1, '+');
    }

    char *dmngl = nullptr;
    if (found1 && found2)
    {
        char orig = *found2;
        // Temporary set null termination because
        // cxa_demangle needs null terminated string
        *found2 = 0;
        int status;
        dmngl   = abi::__cxa_demangle(found1, buff, &len, &status);
        *found2 = orig;
    }

    if (dmngl) // if dmngl is present, found1 and found2 are present too
    {
        buff = dmngl; // the buffer is possibly realloc-ated

        demangled.data[0].iov_base = mangled;
        demangled.data[0].iov_len  = static_cast<size_t>(found1 - mangled);
        demangled.data[1].iov_base = dmngl;
        demangled.data[1].iov_len  = strlen(dmngl);
        demangled.data[2].iov_base = found2;
        demangled.data[2].iov_len  = strlen(found2);
        demangled.size             = 3;
    }
    else
    {
        demangled.data[0].iov_base = mangled;
        demangled.data[0].iov_len  = strlen(mangled);
        demangled.size             = 1;
    }
}

void no_demangle(char *mangled, demangled_info &demangled)
{
    demangled.data[0].iov_base = mangled;
    demangled.data[0].iov_len  = strlen(mangled);
    demangled.size             = 1;
}

void add_EOL(demangled_info &demangled)
{
    size_t i                   = demangled.size;
    demangled.data[i].iov_base = const_cast<char *>("\n");
    demangled.data[i].iov_len  = 1;
    demangled.size += 1;
}

////////////////////////////////////////////////////////////////////////////////

bool dump_stacktrace(int output_fd, const char *info, size_t info_len)
{
    constexpr int err_code = -1;

    if (write(output_fd, info, info_len) == err_code)
        return false;

    {
        const char info[] = "--- STACK TRACE BEG ---\n";
        if (write(output_fd, info, sizeof(info) - 1) == err_code)
        { // There is nothing more we can do here,
            // if we can't even write to the output
            return false;
        }
    }

    enum
    {
        stacktrace_max_levels = 100
    };
    void *stack[stacktrace_max_levels] = {0};
    int count = backtrace(stack, stacktrace_max_levels);
    if (count <= 0)
    {
        const char info[] = "no symbols. backtrace call failed\n";
        auto r = write(output_fd, info, sizeof(info) - 1);
        (void)r;
        return false;
    }

    // Sometimes the backtrace_symbols may hung up because it uses
    // internally malloc. In this case the program gets killed by the
    // alarm signal (see the caller of this function).
    // However, in order to have some information, we first print out
    // the mangled symbols. This call should always succeed.
    // Then we try to print out the mangled symbols which may fail.
    backtrace_symbols_fd(stack, count, output_fd);

    bool res = true;

    /*
    {
        const char delim[] = "********************************\n";
        auto r = write(output_fd, delim, sizeof(delim)-1);
        r = write(output_fd, delim, sizeof(delim)-1);
        (void)r;
    }

    // The bellow code is an attempt to print out the stack
    // with demangled functions.
    char** st = backtrace_symbols(stack, count);
    if (!st)
    {
        const char info[] = "no symbols. backtrace_symbols call failed\n";
        auto r = write(output_fd, info, sizeof(info)-1);
        (void)r;
        return false;
    }

    // dump the stacktrace to the output
    enum { buff_size = 256 };
    demangled_info demangled;
    if (char* buff = static_cast<char*>(malloc(buff_size)))
    {
        for (int i = 0; i < count; ++i)
        {
            demangle(st[i], buff, buff_size, demangled);
            add_EOL(demangled);
            res &= (writev(output_fd, demangled.data, demangled.size) !=
                    err_code);
        }
        free(buff);
    }
    else
    {	// no buffer for demangle, so no demangle
        for (int i = 0; i < count; ++i)
        {
            no_demangle(st[i], demangled);
            add_EOL(demangled);
            res &= (writev(output_fd, demangled.data, demangled.size) !=
                    err_code);
        }
    }

    free(st);
    */

    {
        const char info[] = "--- STACK TRACE END ---\n";
        auto r = write(output_fd, info, sizeof(info) - 1);
        (void)r;
    }

    return res;
}

void write_err_msg(int fd, const char *msg_hdr, const char *info)
{
    char msg[128];
    int msg_len = 0;
    if (msg_hdr && (msg_hdr[0] != 0))
    {
        msg_len =
            snprintf(msg, sizeof(msg), "x3me_err: %s %s\n", msg_hdr, info);
    }
    else
    {
        msg_len = snprintf(msg, sizeof(msg), "x3me_err: %s\n", info);
    }
    if (msg_len > 0)
    {
        auto r = write(fd, msg, msg_len);
        (void)r;
    }
}

////////////////////////////////////////////////////////////////////////////////
// SIGQUIT - is a user generated signal (kind of)
#define FATAL_SIGNALS(MACRO)                                                   \
    MACRO(SIGABRT)                                                             \
    MACRO(SIGFPE)                                                              \
    MACRO(SIGILL)                                                              \
    MACRO(SIGSEGV)                                                             \
    MACRO(SIGTRAP)                                                             \
    MACRO(SIGSYS)                                                              \
    MACRO(SIGBUS)                                                              \
    MACRO(SIGXCPU)                                                             \
    MACRO(SIGXFSZ)

static constexpr int invalid_fd = -1;
// The idea is this file descriptor to be set only once
// and used inside the signal handler.
// Reseting the file descriptor to another value won't be permitted.
// This file descriptor won't be ever closed (i.e. it is leaking)
// but this is not a problem because
// it lives for the whole life of the program
static int g_output_fd = invalid_fd;
static int g_dmesg_fd  = invalid_fd;

volatile sig_atomic_t g_fatal_signal_in_progress = false;

static char g_kern_msg[32];

void fatal_signal_handler(int sig)
{
    // Set the default handler for the received signal.
    // In this way the subsequent raise calls will call the default handler
    signal(sig, SIG_DFL);

    // Since this handler is established for more than one kind of signal,
    // it might still get invoked recursively by delivery of some other kind
    // of signal.  Use a static variable to keep track of that.
    if (g_fatal_signal_in_progress)
        raise(sig);
    g_fatal_signal_in_progress = true;

    // In some situations the program can get stuck
    // in malloc waiting for a lock
    // that it held when the signal is rised. We set an alarm so that
    // if this situation happens this call will allow the program to exit.
    alarm(10);

    const char *info = "!!! CAUGHT NOT EXPECTED SIGNAL !!!";
    switch (sig)
    {
#define CASE_ITERATOR(sig)                                                     \
    case sig:                                                                  \
        info = "!!! CAUGHT " #sig " !!!";                                      \
        break;

        FATAL_SIGNALS(CASE_ITERATOR)

#undef CASE_ITERATOR
    }

    write_err_msg(g_dmesg_fd, g_kern_msg, info);

    dump_stacktrace(g_output_fd, info, strlen(info));

    // Now reraise the signal to its default handler.
    raise(sig);
}

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

bool dump_stacktrace_on_fatal_signal(int output_fd,
                                     const char *kern_msg /*= nullptr*/)
{
    // the function can be called only once
    if (detail::g_output_fd != detail::invalid_fd)
    {
        X3ME_ASSERT(false);
        return false;
    }

    if (kern_msg)
    {
        size_t len     = strlen(kern_msg);
        size_t max_len = sizeof(detail::g_kern_msg) - 1;
        X3ME_ASSERT(len <= max_len);
        strncpy(detail::g_kern_msg, kern_msg, (len < max_len ? len : max_len));
    }
    else
    {
        memset(detail::g_kern_msg, 0, sizeof(detail::g_kern_msg));
    }

#define SUBSCRIBE_ITERATOR(sig)                                                \
    if (signal(sig, detail::fatal_signal_handler) == SIG_ERR)                  \
        return false;

    FATAL_SIGNALS(SUBSCRIBE_ITERATOR)

#undef SUBSCRIBE_ITERATOR

    const int dmesg_fd = ::open("/dev/kmsg", O_WRONLY);
    if (dmesg_fd == -1)
        return false;

    detail::g_output_fd = output_fd;
    detail::g_dmesg_fd  = dmesg_fd;

    return true;
}

} // namespace sys_utils
} // namespace x3me

#undef FATAL_SIGNALS
