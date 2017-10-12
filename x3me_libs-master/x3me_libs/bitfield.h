#pragma once

#include <iosfwd>

namespace x3me
{
namespace bt_utils
{

class bytes_to_bits
{
    const uint8_t* data_;
    uint32_t size_;

public:
    bytes_to_bits(const uint8_t* data, uint32_t size) noexcept : data_(data),
                                                                 size_(size)
    {
        assert(data);
        assert(size > 0);
    }
    template <typename Cont>
    explicit bytes_to_bits(const Cont& c)
        : bytes_to_bits(c.data(), c.size())
    {
    }

    // The GCC 4.8.2 doesn't fully support auto return type
    const uint8_t* begin() const noexcept { return data_; }
    const uint8_t* end() const noexcept { return data_ + size_; }
    const uint8_t* cbegin() const noexcept { return data_; }
    const uint8_t* cend() const noexcept { return data_ + size_; }
    uint32_t size() const noexcept { return size_; }
};

class bitfield_bits
{
    const uint8_t* data_;
    uint32_t bit_size_;

public:
    bitfield_bits(const uint8_t* data, uint32_t bit_size) noexcept
        : data_(data),
          bit_size_(bit_size)
    {
        assert(data);
        assert(bit_size > 0);
    }

    // The GCC 4.8.2 doesn't fully support auto return type
    const uint8_t* data() const noexcept { return data_; }
    uint32_t size() const noexcept { return bit_size_; }
};

////////////////////////////////////////////////////////////////////////////////

class bitfield
{

    uint8_t* data_     = nullptr;
    uint32_t bit_size_ = 0;

public:
    // The user of this class may use these typedefs to make
    // the code a little clear (IMO).
    using bytes_to_bits_t = bytes_to_bits;
    using bitfield_bits_t = bitfield_bits;

public:
    bitfield() noexcept = default;

    explicit bitfield(uint32_t bit_size, bool val = false) noexcept;

    explicit bitfield(const bytes_to_bits& cont) noexcept { assign(cont); }

    explicit bitfield(const bitfield_bits& cont) noexcept { assign(cont); }

    bitfield(bitfield&& rhs) noexcept : data_(rhs.data_),
                                        bit_size_(rhs.bit_size_)
    {
        rhs.data_     = nullptr;
        rhs.bit_size_ = 0;
    }

    bitfield& operator=(bitfield&& rhs) noexcept
    {
        assert(this != &rhs);
        std::free(data_);
        data_         = rhs.data_;
        bit_size_     = rhs.bit_size_;
        rhs.data_     = nullptr;
        rhs.bit_size_ = 0;
        return *this;
    }

    // Add when needed
    bitfield(const bitfield&) = delete;
    bitfield& operator=(const bitfield&) = delete;

    ~bitfield() noexcept { clear(); }

    // NOTE that the bitfield is rarely resized, very rarely.
    // That's why it doesn't use the usual strategy to increase the
    // allocated memory by factor of two.
    // It always takes as much memory as it's exactly needed.
    // Only if the bitfield size increases the new bits will be set to val.
    // The trailing bits from the last byte are always set to 0.
    void resize(uint32_t bit_size, bool val) noexcept;

    void assign(const bytes_to_bits& cont) noexcept;
    void assign(const bitfield_bits& cont) noexcept;

    void append(const bitfield_bits& cont) noexcept;

    void clear() noexcept;

    void set_bit(uint32_t bit, bool val) noexcept;
    bool bit(uint32_t bit) const noexcept;

    explicit operator bool() const noexcept { return bit_size_ != 0; }
    bool empty() const noexcept { return bit_size_ == 0; }
    const uint8_t* data() const { return data_; }
    uint32_t size() const noexcept { return bit_size_; }
    uint32_t byte_size() const noexcept { return byte_size(bit_size_); }

    // Could be used for unsafe fill of the bitfield if it has the needed
    // capacity. The bit_size could be additionally.
    uint8_t* data() noexcept { return data_; }

    bool all() const noexcept;
    bool any() const noexcept;
    bool none() const noexcept { return !any(); }

private:
    void reset(uint32_t bit_size) noexcept;

    // The difference between reset and resize is that the resize
    // preserves the old data pattern in the buffer due to realloc usage
    void resize(uint32_t bit_size) noexcept;

    void clear_trailing_bits() noexcept;

    static void set_bit_1(uint8_t* data, uint32_t bit) noexcept
    {
        data[bit / 8] |= (0x80U >> (bit & 7));
    }
    static void set_bit_0(uint8_t* data, uint32_t bit) noexcept
    {
        data[bit / 8] &= ~(0x80U >> (bit & 7));
    }
    static bool get_bit(const uint8_t* data, uint32_t bit) noexcept
    {
        return (data[bit / 8] & (0x80U >> (bit & 7))) != 0;
    }

    static uint32_t byte_size(uint32_t bit_size) noexcept
    {
        return (bit_size + 7) / 8;
    }
};

std::ostream& operator<<(std::ostream& os, const bitfield& rhs);

} // namespace bt_utils
} // namespace x3me
