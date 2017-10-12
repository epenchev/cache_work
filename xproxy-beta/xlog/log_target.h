#pragma once

#include "xlog_common.h"

namespace xlog
{
namespace detail
{
struct log_target_impl;
}

/// These callback function receives the current log file path and is expected
/// to return the new log file path. The function may (re)move, zip or do
/// whatever is needed with the current log.
/// N.B. The function is going to be called from the special log thread in
/// case of asynchronous logging.
using on_pre_rotate_cb_t = std::function<std::string(const std::string&)>;

////////////////////////////////////////////////////////////////////////////////

class log_target
{
    using impl_t = std::unique_ptr<detail::log_target_impl>;

    template <typename Tag>
    friend class logger;
    friend class async_channel;
    impl_t impl_;

public:
    // We need to defined most of the 'special' functions in the .cpp file
    // in order to avoid inclusion of the implementation in the header file.
    log_target() noexcept = default;
    explicit log_target(impl_t&& impl) noexcept;
    ~log_target() noexcept;

    log_target(log_target&& rhs) noexcept;
    log_target& operator=(log_target&& rhs) noexcept;

    log_target(const log_target&) = delete;
    log_target& operator=(const log_target&) = delete;

    explicit operator bool() const noexcept { return !!impl_; }

#ifdef X3ME_TEST
    auto impl() noexcept { return impl_.get(); }
#endif
};

log_target create_file_target(const char* file_path, bool truncate,
                              level max_lvl, err_code_t& err) noexcept;
log_target create_file_rotate_target(const char* file_path, bool truncate,
                                     uint64_t max_file_size, level max_lvl,
                                     const on_pre_rotate_cb_t& cb,
                                     err_code_t& err) noexcept;
log_target create_file_sliding_target(const char* file_path,
                                      uint64_t max_file_size,
                                      uint32_t size_tolerance, level max_lvl,
                                      err_code_t& err) noexcept;
log_target create_syslog_target(level max_lvl, err_code_t& err) noexcept;

} // namespace xlog
