#include "precompiled.h"
#include "log_target_impl.h"

namespace xlog
{
namespace detail
{

file_target::file_target(log_file&& file, level max_lvl) noexcept
    : file_(std::move(file)),
      max_lvl_(to_number(max_lvl))
{
}

file_target::~file_target() noexcept
{
}

void file_target::write(hdr_data_t& hd) noexcept
{
    err_code_t err;
    const bool r = file_.write(hd, err);
    if (X3ME_UNLIKELY(r == false))
    {
        std::cerr << "xlog::file_target::write failed. " << err.message()
                  << '\n';
    }
}

////////////////////////////////////////////////////////////////////////////////

file_rotate_target::file_rotate_target(log_file&& file, const char* file_path,
                                       uint64_t max_size, uint64_t curr_size,
                                       const on_pre_rotate_cb_t& pre_rotate_cb,
                                       level max_lvl) noexcept
    : file_(std::move(file)),
      log_size_(curr_size),
      on_pre_rotate_cb_(pre_rotate_cb),
      file_path_(file_path),
      max_size_(max_size),
      max_lvl_(to_number(max_lvl))
{
}

file_rotate_target::~file_rotate_target() noexcept
{
}

void file_rotate_target::write(hdr_data_t& hd) noexcept
{
    err_code_t err;
    const bool r = file_.write(hd, err);
    if (X3ME_LIKELY(r == false))
    {
        std::cerr << "xlog::file_rotate_target::write failed. File_path "
                  << file_path_ << ". " << err.message() << '\n';
        return;
    }
    log_size_ += full_size(hd);
    if (log_size_ >= max_size_)
    {
        err_code_t err;
        file_.close(err); // Skip this error, if any

        auto new_path = on_pre_rotate_cb_(file_path_);

        log_size_ = 0;
        file_path_.clear();
        err.clear();

        if (file_.open(new_path.c_str(), true /*truncate*/, err))
            file_path_ = std::move(new_path);
        else
        {
            std::cerr
                << "xlog::file_rotate_target::open_on_rotate failed. File_path "
                << new_path << ". " << err.message() << '\n';
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

file_sliding_target::file_sliding_target(log_file&& file, uint64_t max_size,
                                         uint64_t curr_size,
                                         uint32_t size_tolerance,
                                         uint32_t file_block_size,
                                         level max_lvl) noexcept
    : file_(std::move(file)),
      log_size_(curr_size),
      max_size_(max_size),
      size_tolerance_(size_tolerance),
      file_block_size_(file_block_size),
      max_lvl_(to_number(max_lvl))
{
    X3ME_ENFORCE(size_tolerance < (max_size / 2));
    X3ME_ENFORCE(size_tolerance >= file_block_size);
    X3ME_ENFORCE((size_tolerance % file_block_size) == 0);
}

file_sliding_target::~file_sliding_target() noexcept
{
}

void file_sliding_target::write(hdr_data_t& hd) noexcept
{
    err_code_t err;
    const bool r = file_.write(hd, err);
    if (X3ME_LIKELY(r == false))
    {
        std::cerr << "xlog::file_sliding_target::write failed. "
                  << err.message() << '\n';
        return;
    }
    log_size_ += full_size(hd);
    if (log_size_ >= max_size_)
    {
        using x3me::math::round_up_pow2;
        const auto min_size = max_size_ - size_tolerance_;
        const auto rem_bytes =
            round_up_pow2(log_size_ - min_size, file_block_size_);
        // N.B. We set accounted log_size to min_size even if the below call
        // fails, which shouldn't happen in practice. This way we won't enter
        // here all the time in case of failure.
        // If the call fail once it'll more likely fail every
        // time and we don't want this to happen for every write call.
        assert(log_size_ > rem_bytes);
        log_size_ -= rem_bytes;
        // Remove N bytes from the beginning. We are currently OK with the
        // fact that the first record may remain partial.
        err_code_t err;
        if (!file_.remove_range(0, rem_bytes, err))
        {
            std::cerr << "xlog::file_sliding_target::sliding failed. "
                      << err.message() << '\n';
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

syslog_target::syslog_target(log_file&& file, level max_lvl) noexcept
    : file_(std::move(file)),
      max_lvl_(to_number(max_lvl))
{
}

syslog_target::~syslog_target() noexcept
{
}

void syslog_target::write(hdr_data_t& hd) noexcept
{
    err_code_t err;
    const bool r = file_.write(hd, err);
    if (X3ME_UNLIKELY(r == false))
    {
        std::cerr << "Unable to write to dmesg. " << err.message() << '\n';
    }
}

} // namespace detail
} // namespace xlog
