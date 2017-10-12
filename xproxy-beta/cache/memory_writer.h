#pragma once

namespace cache
{
namespace detail
{

class memory_writer
{
    uint8_t* buf_;
    bytes64_t buf_offs_;
    const bytes64_t buf_size_;

public:
    memory_writer(uint8_t* buf,
                  bytes64_t buf_size,
                  bytes64_t init_offset = 0) noexcept : buf_(buf),
                                                        buf_offs_(init_offset),
                                                        buf_size_(buf_size)
    {
        X3ME_ENFORCE(init_offset < buf_size,
                     "The offset must be somewhere inside the buffer");
    }
    ~memory_writer() noexcept = default;

    memory_writer(const memory_writer&) = delete;
    memory_writer& operator=(const memory_writer&) = delete;
    memory_writer(memory_writer&&) = delete;
    memory_writer& operator=(memory_writer&&) = delete;

    // The provided offset must be in [curr_offset, buf_size_)
    void set_next_offset(bytes64_t offs) noexcept
    {
        X3ME_ENFORCE(x3me::math::in_range(offs, buf_offs_, buf_size_),
                     "The offset must be somewhere inside the current limits");
        buf_offs_ = offs;
    }

    void write(const void* buf, size_t len) noexcept
    {
        X3ME_ENFORCE((len <= buf_size_) && (buf_offs_ <= (buf_size_ - len)),
                     "The buffer can't hold the provided data");
        ::memcpy(buf_ + buf_offs_, buf, len);
        buf_offs_ += len;
    }

    bytes64_t buff_size() const noexcept { return buf_size_; }
    bytes64_t written() const noexcept { return buf_offs_; }
};

} // namespace detail
} // namespace cache
