#include "precompiled.h"
#include "read_buffers.h"

namespace cache
{
namespace detail
{

read_buffers::read_buffers(read_buffers&& rhs) noexcept
    : bufs_(std::move(rhs.bufs_)),
      curr_idx_(std::exchange(rhs.curr_idx_, 0)),
      curr_offs_(std::exchange(rhs.curr_offs_, 0)),
      bytes_read_(std::exchange(rhs.bytes_read_, 0))
{
}

read_buffers& read_buffers::operator=(read_buffers&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        bufs_       = std::move(rhs.bufs_);
        curr_idx_   = std::exchange(rhs.curr_idx_, 0);
        curr_offs_  = std::exchange(rhs.curr_offs_, 0);
        bytes_read_ = std::exchange(rhs.bytes_read_, 0);
    }
    return *this;
}

read_buffers& read_buffers::operator=(buffers&& rhs) noexcept
{
    bufs_       = std::move(rhs);
    curr_idx_   = 0;
    curr_offs_  = 0;
    bytes_read_ = 0;
    return *this;
}

bytes64_t read_buffers::read(buffer_t buff) noexcept
{
    bytes64_t bytes_read = 0;

    uint8_t* ptr  = buff.data();
    bytes64_t len = buff.size();

    const auto bdata = bufs_.data();

    auto offs = curr_offs_;
    auto idx = curr_idx_;
    while ((idx < bdata.size()) && (len > 0))
    {
        iovec& iov = bdata[idx];

        const auto to_copy = std::min(iov.iov_len - offs, len);

        ::memcpy(ptr, static_cast<const uint8_t*>(iov.iov_base) + offs,
                 to_copy);

        ptr += to_copy;
        len -= to_copy;

        offs += to_copy;
        if (offs == iov.iov_len)
        {
            offs = 0;
            ++idx;
        }

        bytes_read += to_copy;
    }

    curr_idx_  = idx;
    curr_offs_ = offs;
    bytes_read_ += bytes_read;

    return bytes_read;
}

bytes64_t read_buffers::skip_read(bytes64_t len) noexcept
{
    bytes64_t bytes_read = 0;

    const auto bdata = bufs_.data();

    auto offs = curr_offs_;
    auto idx = curr_idx_;
    while ((idx < bdata.size()) && (len > 0))
    {
        iovec& iov = bdata[idx];

        const auto to_copy = std::min(iov.iov_len - offs, len);

        len -= to_copy;

        offs += to_copy;
        if (offs == iov.iov_len)
        {
            offs = 0;
            ++idx;
        }

        bytes_read += to_copy;
    }

    curr_idx_  = idx;
    curr_offs_ = offs;
    bytes_read_ += bytes_read;

    return bytes_read;
}

void read_buffers::swap(read_buffers& rhs) noexcept
{
    using std::swap;
    bufs_.swap(rhs.bufs_);
    swap(curr_idx_, rhs.curr_idx_);
    swap(curr_offs_, rhs.curr_offs_);
    swap(bytes_read_, rhs.bytes_read_);
}

bool read_buffers::all_read() const noexcept
{
    // Intentionally return true in case of empty buffers
    return (curr_idx_ == bufs_.size()) && (curr_offs_ == 0);
}

} // namespace detail
} // namespace cache
