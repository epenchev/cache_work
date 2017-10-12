#pragma once

#include "xlog_common.h"
#include "log_file.h"

namespace xlog
{
namespace detail
{

////////////////////////////////////////////////////////////////////////////////

struct log_target_impl
{
    virtual ~log_target_impl() noexcept {}
    virtual void write(hdr_data_t& hd) noexcept = 0;
    virtual level_type max_log_level() const noexcept = 0;
};

////////////////////////////////////////////////////////////////////////////////

class file_target final : public log_target_impl
{
    log_file file_;
    const level_type max_lvl_;

public:
    file_target(log_file&& file, level max_lvl) noexcept;
    ~file_target() noexcept final;
    void write(hdr_data_t& hd) noexcept final;
    level_type max_log_level() const noexcept final { return max_lvl_; }
};

////////////////////////////////////////////////////////////////////////////////

class file_rotate_target final : public log_target_impl
{
    using on_pre_rotate_cb_t = std::function<std::string(const std::string&)>;

    log_file file_;
    uint64_t log_size_;
    on_pre_rotate_cb_t on_pre_rotate_cb_;
    std::string file_path_;
    const uint64_t max_size_;
    const level_type max_lvl_;

public:
    file_rotate_target(log_file&& file, const char* file_path,
                       uint64_t max_size, uint64_t curr_size,
                       const on_pre_rotate_cb_t& pre_rotate_cb,
                       level max_lvl) noexcept;
    ~file_rotate_target() noexcept final;
    void write(hdr_data_t& hd) noexcept final;
    level_type max_log_level() const noexcept final { return max_lvl_; }
};

////////////////////////////////////////////////////////////////////////////////

class file_sliding_target final : public log_target_impl
{
    log_file file_;
    uint64_t log_size_;
    const uint64_t max_size_;
    const uint32_t size_tolerance_;
    const uint32_t file_block_size_;
    const level_type max_lvl_;

public:
    file_sliding_target(log_file&& file, uint64_t max_size, uint64_t curr_size,
                        uint32_t size_tolerance, uint32_t file_block_size,
                        level max_lvl) noexcept;
    ~file_sliding_target() noexcept final;
    void write(hdr_data_t& hd) noexcept final;
    level_type max_log_level() const noexcept final { return max_lvl_; }
};

////////////////////////////////////////////////////////////////////////////////

class syslog_target final : public log_target_impl
{
    log_file file_;
    const level_type max_lvl_;

public:
    syslog_target(log_file&& file, level max_lvl) noexcept;
    ~syslog_target() noexcept final;
    void write(hdr_data_t& hd) noexcept final;
    level_type max_log_level() const noexcept final { return max_lvl_; }
};

} // namespace detail
} // namespace xlog
