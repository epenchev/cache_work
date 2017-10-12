#pragma once

namespace cache
{
namespace detail
{

class memory_reader
{
    const uint8_t* buf_;
    bytes64_t buf_offs_;
    const bytes64_t buf_size_;

public:
    memory_reader(const uint8_t* buf, bytes64_t buf_size,
                  bytes64_t init_offset = 0) noexcept : buf_(buf),
                                                        buf_offs_(init_offset),
                                                        buf_size_(buf_size)
    {
        X3ME_ENFORCE(init_offset < buf_size,
                     "The offset must be somewhere inside the buffer");
    }
    ~memory_reader() noexcept = default;

    memory_reader(const memory_reader&) = delete;
    memory_reader& operator=(const memory_reader&) = delete;
    memory_reader(memory_reader&&) = delete;
    memory_reader& operator=(memory_reader&&) = delete;

    // The provided offset must be in [curr_offset, buf_size_)
    void set_next_offset(bytes64_t offs) noexcept
    {
        X3ME_ENFORCE(x3me::math::in_range(offs, buf_offs_, buf_size_),
                     "The offset must be somewhere inside the current limits");
        buf_offs_ = offs;
    }

    void read(void* buf, size_t len) noexcept
    {
        X3ME_ENFORCE((len <= buf_size_) && (buf_offs_ <= (buf_size_ - len)),
                     "The buffer can't provide so much data");
        ::memcpy(buf, buf_ + buf_offs_, len);
        buf_offs_ += len;
    }
};

} // namespace detail
} // namespace cache
