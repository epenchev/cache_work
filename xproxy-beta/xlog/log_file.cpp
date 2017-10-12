#include "precompiled.h"
#include "log_file.h"

using boost::system::system_category;

namespace xlog
{
namespace detail
{

log_file::~log_file() noexcept
{
    err_code_t skip;
    close(skip);
}

log_file::log_file(log_file&& rhs) noexcept : fd_(rhs.fd_)
{
    rhs.fd_ = invalid_fd;
}

bool log_file::open(const char* file_path, bool truncate,
                    err_code_t& err) noexcept
{
    X3ME_ENFORCE(fd_ == invalid_fd);
    const auto flags =
        truncate ? (O_CREAT | O_TRUNC | O_RDWR) : (O_CREAT | O_APPEND | O_RDWR);
    const auto mode = S_IRUSR | S_IWUSR; // | S_IRGRP | S_IROTH;
    int fd = ::open(file_path, flags, mode);
    if (fd == invalid_fd)
    {
        err.assign(errno, system_category());
        return false;
    }
    fd_ = fd;
    return true;
}

bool log_file::close(err_code_t& err) noexcept
{
    bool res = true;
    if (fd_ != invalid_fd)
    {
        const auto r = ::close(fd_);
        fd_ = invalid_fd;
        if (r != 0)
        {
            res = false;
            err.assign(errno, system_category());
        }
    }
    return res;
}

bool log_file::write(const char* data, uint32_t size, err_code_t& err) noexcept
{
    for (int64_t left_bytes = size; left_bytes > 0;)
    {
        const ssize_t written = ::write(fd_, data, left_bytes);
        if (written == 0)
        {
            err.assign(EINVAL, system_category());
            return false;
        }
        if (written < 0)
        {
            err.assign(errno, system_category());
            return false;
        }
        data += written;
        left_bytes -= written;
    }
    return true;
}

bool log_file::write(hdr_data_t& data, err_code_t& err) noexcept
{
    static_assert(x3me::utils::size(hdr_data_t{}) == 2, "");
    // Although the vectored write operation will probably write
    // all data with one call, we need to be able to handle the case
    // when the data are written with multiple calls because it's possible.
    const size_t hdr_size = data[0].iov_len;
    const size_t all_size = data[0].iov_len + data[1].iov_len;
    for (size_t all_written = 0; all_written < all_size;)
    {
        const size_t tmp = (all_written < hdr_size) ? 0 : 1;
        const size_t idx = tmp;
        const size_t ofs = all_written - (tmp * hdr_size);
        const size_t cnt = data.size() - tmp;

        auto& b    = data[idx];
        b.iov_base = static_cast<char*>(b.iov_base) + ofs;
        b.iov_len -= ofs;

        const ssize_t written = ::writev(fd_, &b, cnt);

        if (written == 0)
        {
            err.assign(EINVAL, system_category());
            return false;
        }
        if (written < 0)
        {
            err.assign(errno, system_category());
            return false;
        }

        all_written += written;
        assert(all_written <= all_size);
    }
    return true;
}

int64_t log_file::size(err_code_t& err) const noexcept
{
    struct stat s;
    if (fstat(fd_, &s) == 0)
    {
        return s.st_size;
    }
    err.assign(errno, system_category());
    return -1;
}

uint32_t log_file::block_size(err_code_t& err) const noexcept
{
    struct stat s;
    if (fstat(fd_, &s) == 0)
    {
        return s.st_blksize;
    }
    err.assign(errno, system_category());
    return -1;
}

bool log_file::remove_range(uint64_t beg, uint64_t size,
                            err_code_t& err) noexcept
{
    // N.B. The call to fallocate with this flag requires EXT-4/XFS from
    // Linux kernel > 3.15. Thus we check the version here, once.
    static bool unused = []
    {
        using namespace x3me::sys_utils;
        kern_ver kv;
        if (kernel_version(kv) &&
            ((kv.version_ < 3) ||
             ((kv.version_ == 3) && (kv.major_rev_ <= 15))))
        {
            std::cerr << "Kernel version needs to be greater than 3.15 if you "
                         "want to support remove file range logic\n";
        }
        return true;
    };
    (void)unused;
#ifndef FALLOC_FL_COLLAPSE_RANGE
#define FALLOC_FL_COLLAPSE_RANGE 0x08
#endif
    if (fallocate(fd_, FALLOC_FL_COLLAPSE_RANGE, beg, size) < 0)
    {
        err.assign(errno, system_category());
        return false;
    }
    return true;
}

} // namespace detail
} // namespace xlog
