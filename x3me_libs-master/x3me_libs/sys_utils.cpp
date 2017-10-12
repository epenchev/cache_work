#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sys_utils.h"

namespace x3me
{
namespace sys_utils
{

unsigned int process_id() noexcept
{
    return getpid();
}

unsigned int thread_id() noexcept
{
    return syscall(SYS_gettid);
}

unsigned int memory_page_size() noexcept
{
    return sysconf(_SC_PAGESIZE);
}

cpu_times cpu_usage_times() noexcept
{
    cpu_times ret = {0, 0};
    rusage rusg;
    memset(&rusg, 0, sizeof(rusg));
    if (getrusage(RUSAGE_SELF, &rusg) == 0)
    {
        auto timeval_to_secs = [](const timeval& t)
        {
            return (static_cast<double>(t.tv_sec) +
                    (static_cast<double>(t.tv_usec) / 1000000.0));
        };
        ret.secs_kernel_mode = timeval_to_secs(rusg.ru_stime);
        ret.secs_user_mode   = timeval_to_secs(rusg.ru_utime);
    }
    return ret;
}

mem_usage memory_usage() noexcept
{
    mem_usage ret = {0, 0, 0};
    rusage rusg;
    memset(&rusg, 0, sizeof(rusg));
    if (getrusage(RUSAGE_SELF, &rusg) == 0)
    {
        ret.ress_max_size = (rusg.ru_maxrss * 1024ULL); // KBytes to Bytes
    }
    if (auto f = ::fopen("/proc/self/statm", "r"))
    {
        unsigned vm_size = 0;
        unsigned rs_size = 0;
        if (fscanf(f, "%u %u", &vm_size, &rs_size) == 2)
        {
            unsigned long long mpg_size = memory_page_size();
            ret.virt_size               = mpg_size * vm_size;
            ret.ress_size               = mpg_size * rs_size;
        }
        fclose(f);
    }
    return ret;
}

bool set_this_thread_name(const char* name) noexcept
{
    return (pthread_setname_np(pthread_self(), name) == 0);
}

bool get_this_thread_name(char* name, size_t len) noexcept
{
    return (pthread_getname_np(pthread_self(), name, len) == 0);
}

bool kernel_version(kern_ver& kv) noexcept
{
    bool res = false;
    utsname un;
    if (uname(&un) == 0)
    {
        res = (sscanf(un.release, "%u.%hu.%hu", &kv.version_, &kv.major_rev_,
                      &kv.minor_rev_) == 3);
    }
    if (!res)
    {
        kv.version_   = 0;
        kv.major_rev_ = 0;
        kv.minor_rev_ = 0;
    }
    return res;
}

} // namespace sys_utils
} // namespace x3me
