#pragma once

namespace x3me
{
namespace sys_utils
{

// The function should be called only once in a given program.
// A call made by two different threads is also not thread safe.
// kern_msg is the begining of the kernel message. It is useful to
// distinguish the crashed application. Expects NULL terminated string
// with max length 31 chars.
bool dump_stacktrace_on_fatal_signal(int output_fd,
                                     const char* kern_msg = nullptr);

} // namespace sys_utils
} // namespace x3me
