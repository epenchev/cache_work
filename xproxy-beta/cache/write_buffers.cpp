#include "precompiled.h"
#include "write_buffers.h"

namespace cache
{
namespace detail
{

write_buffers::write_buffers(write_buffers&& rhs) noexcept
    : bufs_(std::move(rhs.bufs_)),
      curr_idx_(std::exchange(rhs.curr_idx_, 0)),
      curr_offs_(std::exchange(rhs.curr_offs_, 0)),
      bytes_written_(std::exchange(rhs.bytes_written_, 0))
{
}

write_buffers& write_buffers::operator=(write_buffers&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        bufs_          = std::move(rhs.bufs_);
        curr_idx_      = std::exchange(rhs.curr_idx_, 0);
        curr_offs_     = std::exchange(rhs.curr_offs_, 0);
        bytes_written_ = std::exchange(rhs.bytes_written_, 0);
    }
    return *this;
}

write_buffers& write_buffers::operator=(buffers&& rhs) noexcept
{
    bufs_          = std::move(rhs);
    curr_idx_      = 0;
    curr_offs_     = 0;
    bytes_written_ = 0;
    return *this;
}

bytes32_t write_buffers::write(data_t data) noexcept
{
    bytes32_t written = 0;

    const uint8_t* ptr = data.data();
    auto len           = data.size();

    const auto bdata = bufs_.data();

    auto offs = curr_offs_;
    auto idx = curr_idx_;
    while ((idx < bdata.size()) && (len > 0))
    {
        iovec& iov = bdata[idx];

        const auto to_copy = std::min(iov.iov_len - offs, len);

        ::memcpy(static_cast<uint8_t*>(iov.iov_base) + offs, ptr, to_copy);

        ptr += to_copy;
        len -= to_copy;

        offs += to_copy;
        if (offs == iov.iov_len)
        {
            offs = 0;
            ++idx;
        }

        written += to_copy;
    }

    curr_idx_  = idx;
    curr_offs_ = offs;
    bytes_written_ += written;

    return written;
}

void write_buffers::swap(write_buffers& rhs) noexcept
{
    using std::swap;
    bufs_.swap(rhs.bufs_);
    swap(curr_idx_, rhs.curr_idx_);
    swap(curr_offs_, rhs.curr_offs_);
    swap(bytes_written_, rhs.bytes_written_);
}

bool write_buffers::all_written() const noexcept
{
    // Intentionally return true in case of empty buffers
    return (curr_idx_ == bufs_.size()) && (curr_offs_ == 0);
}

} // namespace detail
} // namespace cache
