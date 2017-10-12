#pragma once

#include "buffer.h"

namespace cache
{
namespace detail
{

class write_buffers
{
    buffers bufs_;
    // The index of the current buffer from where to resume writing to.
    uint32_t curr_idx_ = 0;
    // The offset in the current buffer from where to resume writing to.
    bytes32_t curr_offs_ = 0;
    // All bytes written to the buffers so far.
    bytes32_t bytes_written_ = 0;

public:
    write_buffers() noexcept = default;
    ~write_buffers() noexcept = default;

    write_buffers(const write_buffers&) = delete;
    write_buffers& operator=(const write_buffers&) = delete;

    write_buffers(write_buffers&&) noexcept;
    write_buffers& operator=(write_buffers&&) noexcept;

    write_buffers& operator=(buffers&& rhs) noexcept;

    using data_t = x3me::mem_utils::array_view<const uint8_t>;
    // Returns the written bytes.
    bytes32_t write(data_t data) noexcept;

    void swap(write_buffers& rhs) noexcept;

    bool all_written() const noexcept;

    bytes32_t bytes_written() const noexcept { return bytes_written_; }

    bool empty() const noexcept { return bufs_.empty(); }
};

} // namespace detail
} // namespace cache
