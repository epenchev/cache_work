#pragma once

namespace x3me
{
namespace sys_utils
{

unsigned int process_id() noexcept;
unsigned int thread_id() noexcept;
unsigned int memory_page_size() noexcept;

struct cpu_times
{
    double secs_user_mode;
    double secs_kernel_mode;
};
cpu_times cpu_usage_times() noexcept;

struct mem_usage // All stats in bytes
{
    unsigned long long virt_size;
    unsigned long long ress_size;
    unsigned long long ress_max_size;
};
mem_usage memory_usage() noexcept;

// The function returns false if the name length is > than 15
bool set_this_thread_name(const char* name) noexcept;
// The function returns false if the len is < than 16.
bool get_this_thread_name(char* name, size_t len) noexcept;

struct kern_ver
{
    unsigned int version_;
    unsigned short major_rev_;
    unsigned short minor_rev_;
};

// Returns true in case of success.
// Returns false in case of failure and the content of the fields of the
// structure is 0.
bool kernel_version(kern_ver& kv) noexcept;

} // namespace sys_utils
} // namespace x3me
