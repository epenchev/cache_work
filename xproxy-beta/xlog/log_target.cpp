#include "precompiled.h"
#include "log_target.h"
#include "log_target_impl.h"
#include "log_file.h"

namespace xlog
{

log_target::log_target(impl_t&& impl) noexcept : impl_(std::move(impl))
{
}

log_target::~log_target() noexcept
{
}

log_target::log_target(log_target&& rhs) noexcept : impl_(std::move(rhs.impl_))
{
}

log_target& log_target::operator=(log_target&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////

log_target create_file_target(const char* file_path, bool truncate,
                              level max_lvl, err_code_t& err) noexcept
{
    detail::log_file file;
    if (file.open(file_path, truncate, err))
    {
        return log_target{
            std::make_unique<detail::file_target>(std::move(file), max_lvl)};
    }
    assert(err);
    return log_target{};
}

log_target create_file_rotate_target(const char* file_path, bool truncate,
                                     uint64_t max_file_size, level max_lvl,
                                     const on_pre_rotate_cb_t& cb,
                                     err_code_t& err) noexcept
{
    log_target res;
    detail::log_file file;
    if (file.open(file_path, truncate, err))
    {
        assert(!err);
        const auto curr_size = file.size(err);
        if (curr_size >= 0)
        {
            return log_target{std::make_unique<detail::file_rotate_target>(
                std::move(file), file_path, max_file_size, curr_size, cb,
                max_lvl)};
        }
    }
    assert(err);
    return log_target{};
}

log_target create_file_sliding_target(const char* file_path,
                                      uint64_t max_file_size,
                                      uint32_t size_tolerance, level max_lvl,
                                      err_code_t& err) noexcept
{
    log_target res;
    detail::log_file file;
    // The internal functionality used by the sliding target can't work
    // if the file is not opened in append mode.
    if (file.open(file_path, false /*append*/, err))
    {
        assert(!err);
        const auto curr_size = file.size(err);
        if (curr_size >= 0)
        {
            const auto block_size = file.block_size(err);
            if (block_size > 0)
            {
                // Re-adjust the size tolerance, if needed.
                size_tolerance =
                    x3me::math::round_up_pow2(size_tolerance, block_size);
                return log_target{std::make_unique<detail::file_sliding_target>(
                    std::move(file), max_file_size, curr_size, size_tolerance,
                    block_size, max_lvl)};
            }
        }
    }
    assert(err);
    return log_target{};
}

log_target create_syslog_target(level max_lvl, err_code_t& err) noexcept
{
    // The FlashOS doesn't have syslog. The closest thing that we can use
    // is the dmesg. Not sure if it's a good idea to keep a handle to
    // the dmesg opened all the time. We need to open and keep a handle open
    // because the process may relinquish privileges later.
    detail::log_file file;
    if (file.open("/dev/kmsg", false /*truncate*/, err))
    {
        return log_target{
            std::make_unique<detail::syslog_target>(std::move(file), max_lvl)};
    }
    assert(err);
    return log_target{};
}

} // namespace xlog
