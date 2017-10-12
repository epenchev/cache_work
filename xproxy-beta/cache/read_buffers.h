#pragma once

#include "buffer.h"

namespace cache
{
namespace detail
{

class read_buffers
{
    buffers bufs_;
    // The index of the current buffer from where to resume writing to.
    uint32_t curr_idx_ = 0;
    // The offset in the current buffer from where to resume writing to.
    bytes32_t curr_offs_ = 0;
    // All bytes written to the buffers so far.
    bytes32_t bytes_read_ = 0;

public:
    read_buffers() noexcept = default;
    ~read_buffers() noexcept = default;

    read_buffers(const read_buffers&) = delete;
    read_buffers& operator=(const read_buffers&) = delete;

    read_buffers(read_buffers&&) noexcept;
    read_buffers& operator=(read_buffers&&) noexcept;

    read_buffers& operator=(buffers&& rhs) noexcept;

    using buffer_t = x3me::mem_utils::array_view<uint8_t>;
    // Returns the read bytes.
    bytes64_t read(buffer_t buff) noexcept;

    // Tries to skip the number of bytes. Returns the number of actually
    // skipped bytes which is in the range [0, len].
    bytes64_t skip_read(bytes64_t len) noexcept;

    void swap(read_buffers& rhs) noexcept;

    bool all_read() const noexcept;

    bytes32_t bytes_read() const noexcept { return bytes_read_; }

    bool empty() const noexcept { return bufs_.empty(); }

    // TODO Remove it after finding the bug
    friend std::ostream& operator<<(std::ostream& os,
                                    const read_buffers& rhs) noexcept
    {
        return os << "{Bufs_size: " << rhs.bufs_.size()
                  << ", Curr_idx: " << rhs.curr_idx_
                  << ", Curr_offs: " << rhs.curr_offs_
                  << ", Bytes_read: " << rhs.bytes_read_ << '}';
    }
};

} // namespace detail
} // namespace cache
