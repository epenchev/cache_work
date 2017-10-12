#include "precompiled.h"
#include "volume_fd.h"
#include "cache_error.h"

namespace cache
{
namespace detail
{

volume_fd::~volume_fd() noexcept
{
    err_code_t skip;
    close(skip);
}

volume_fd::volume_fd(volume_fd&& rhs) noexcept
    : fd_(std::exchange(rhs.fd_, invalid_fd))
{
}

volume_fd& volume_fd::operator=(volume_fd&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        err_code_t skip;
        close(skip);
        fd_ = std::exchange(rhs.fd_, invalid_fd);
    }
    return *this;
}

bool volume_fd::open(const char* path, err_code_t& err) noexcept
{
    assert(fd_ == invalid_fd);
    const auto flags = O_RDWR | O_DIRECT | O_DSYNC;
    const auto mode  = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    auto fd = ::open(path, flags, mode);
    if (fd == invalid_fd)
    {
        err.assign(errno, bsys::get_system_category());
        return false;
    }
    fd_ = fd;
    return true;
}

bool volume_fd::read(uint8_t* buf,
                     bytes32_t len,
                     bytes64_t off,
                     err_code_t& err) noexcept
{
    for (ssize_t left_bytes = len; left_bytes > 0;)
    {
        const ssize_t read_bytes = ::pread(fd_, buf, left_bytes, off);
        if (read_bytes == 0)
        {
            err.assign(cache::error::eof, get_cache_error_category());
            return false;
        }
        if (read_bytes < 0)
        {
            err.assign(errno, bsys::get_system_category());
            return false;
        }
        buf += read_bytes;
        left_bytes -= read_bytes;
        off += read_bytes;
    }
    return true;
}

bool volume_fd::write(const uint8_t* buf,
                      bytes32_t len,
                      bytes64_t off,
                      err_code_t& err) noexcept
{
    for (ssize_t left_bytes = len; left_bytes > 0;)
    {
        const ssize_t written = ::pwrite(fd_, buf, left_bytes, off);
        if (written == 0)
        {
            // Really don't know if this case can happen in practice and
            // what exactly to do if it happens. Should I retry or report
            // error?
            err.assign(cache::error::null_write, get_cache_error_category());
            return false;
        }
        if (written < 0)
        {
            err.assign(errno, bsys::get_system_category());
            return false;
        }
        buf += written;
        left_bytes -= written;
        off += written;
    }
    return true;
}

bool volume_fd::truncate(bytes64_t size, err_code_t& err) noexcept
{
    if (::ftruncate(fd_, size) < 0)
    {
        err.assign(errno, bsys::get_system_category());
        return false;
    }
    return true;
}

bool volume_fd::close(err_code_t& err) noexcept
{
    if (fd_ != invalid_fd)
    {
        auto fd = fd_;
        fd_ = invalid_fd;
        if (::close(fd) < 0)
        {
            err.assign(errno, bsys::get_system_category());
            return false;
        }
    }
    return true;
}

} // namespace detail
} // namespace cache
