#include <cstdlib>
#include <cstring>
#include <ostream>

#include <boost/algorithm/hex.hpp>

#include "bitfield.h"

namespace x3me
{
namespace bt_utils
{

bitfield::bitfield(uint32_t bit_size, bool val /*= false*/) noexcept
    : data_(static_cast<uint8_t*>(std::malloc(byte_size(bit_size)))),
      bit_size_(bit_size)
{
    assert(bit_size > 0);
    assert(data_);
    if (!val)
    {
        std::memset(data_, 0x00, byte_size(bit_size));
    }
    else
    {
        std::memset(data_, 0xFF, byte_size(bit_size));
        clear_trailing_bits();
    }
}

void bitfield::resize(uint32_t bit_size, bool val) noexcept
{
    const auto old_bit_size      = bit_size_;
    const auto old_trailing_bits = bit_size_ & 7U;
    const auto old_byte_size     = byte_size(bit_size_);
    const auto new_byte_size = byte_size(bit_size);
    if (old_byte_size != new_byte_size)
    {
        // The realloc may return non null pointer when the new size is 0.
        // That's why we use this if.
        if (new_byte_size > 0)
        {
            // realloc preserves the old data in the newly allocated memory
            void* p = std::realloc(data_, new_byte_size);
            assert(p);
            data_ = static_cast<uint8_t*>(p);
        }
        else
        {
            std::free(data_);
            data_ = nullptr;
        }
    }
    bit_size_ = bit_size;
    if (!val)
    {
        // The trailing bytes of the last old byte are set to 0.
        // We don't need to clear them again in neither case.
        if (new_byte_size > old_byte_size)
        {
            std::memset(data_ + old_byte_size, 0x00,
                        (new_byte_size - old_byte_size));
        }
        else
        {
            clear_trailing_bits();
        }
    }
    else
    {
        // Note that if we have some old trailing bits this means
        // that we have at least one byte old size.
        // If the new bit size is <= than the old bit size we need only
        // to clear the trailing bits.
        if ((bit_size > old_bit_size) && old_trailing_bits)
        {
            // We need to preserve the old trailing bits from the last old
            // byte and set the bits after them, in the same byte, to 1.
            data_[old_byte_size - 1] |= (0xFFU >> old_trailing_bits);
        }
        if (new_byte_size > old_byte_size)
        {
            std::memset(data_ + old_byte_size, 0xFF,
                        (new_byte_size - old_byte_size));
        }
        clear_trailing_bits();
    }
}

void bitfield::assign(const bytes_to_bits& cont) noexcept
{
    auto bit_size = cont.size();
    reset(bit_size);
    uint32_t i = 0;
    for (const auto& v : cont)
    {
        if (v != 0)
            set_bit_1(data_, i);
        else
            set_bit_0(data_, i);
        ++i;
    }
    clear_trailing_bits();
}

void bitfield::assign(const bitfield_bits& cont) noexcept
{
    auto bit_size = cont.size();
    reset(bit_size);
    std::memcpy(data_, cont.data(), byte_size(bit_size));
    clear_trailing_bits();
}

void bitfield::append(const bitfield_bits& cont) noexcept
{
    auto bit_size            = cont.size();
    const auto old_bit_size  = bit_size_;
    const auto old_byte_size = byte_size(bit_size_);
    resize(old_bit_size + bit_size);
    if ((old_bit_size & 7U) == 0)
    {
        std::memcpy(data_ + old_byte_size, cont.data(), byte_size(bit_size));
    }
    else
    { // The slow append
        auto data = cont.data();
        for (uint32_t i = 0, j = old_bit_size; i < bit_size; ++i, ++j)
        {
            if (get_bit(data, i))
                set_bit_1(data_, j);
            else
                set_bit_0(data_, j);
        }
    }
    clear_trailing_bits();
}

void bitfield::clear() noexcept
{
    std::free(data_);
    data_     = nullptr;
    bit_size_ = 0;
}

void bitfield::set_bit(uint32_t bit, bool val) noexcept
{
    assert(bit < bit_size_);
    if (val)
        set_bit_1(data_, bit);
    else
        set_bit_0(data_, bit);
}

bool bitfield::bit(uint32_t bit) const noexcept
{
    assert(bit < bit_size_);
    return get_bit(data_, bit);
}

bool bitfield::all() const noexcept
{
    // TODO This function can be make more effective if we traverse
    // with uint64_t step.
    const auto num_bytes = bit_size_ / 8;
    const uint8_t* p = data_;
    for (uint32_t i = 0; i < num_bytes; ++i, ++p)
    {
        if (*p != 0xFF)
            return false;
    }
    auto rest    = bit_size_ - (num_bytes * 8);
    uint8_t mask = (0xFFU << (8 - rest));
    return (rest == 0) || ((data_[num_bytes] & mask) == mask);
}

bool bitfield::any() const noexcept
{
    // TODO This function can be make more effective if we traverse
    // with uint64_t step.
    const auto num_bytes = bit_size_ / 8;
    const uint8_t* p = data_;
    for (uint32_t i = 0; i < num_bytes; ++i, ++p)
    {
        if (*p != 0x00)
            return true;
    }
    auto rest    = bit_size_ - (num_bytes * 8);
    uint8_t mask = (0xFFU << (8 - rest));
    return (rest > 0) && ((data_[num_bytes] & mask) != 0x00);
}

void bitfield::reset(uint32_t bit_size) noexcept
{
    const auto old_byte_size = byte_size(bit_size_);
    const auto new_byte_size = byte_size(bit_size);
    if (old_byte_size != new_byte_size)
    {
        assert(new_byte_size != 0);
        std::free(data_);
        data_ = static_cast<uint8_t*>(std::malloc(new_byte_size));
        assert(data_);
    }
    bit_size_ = bit_size;
}

void bitfield::resize(uint32_t bit_size) noexcept
{
    const auto old_byte_size = byte_size(bit_size_);
    const auto new_byte_size = byte_size(bit_size);
    if (old_byte_size != new_byte_size)
    {
        assert(new_byte_size != 0);
        data_ = static_cast<uint8_t*>(std::realloc(data_, new_byte_size));
        assert(data_);
    }
    bit_size_ = bit_size;
}

void bitfield::clear_trailing_bits() noexcept
{
    const uint8_t trailing_bits = bit_size_ & 7U;
    if (trailing_bits)
        data_[byte_size(bit_size_) - 1] &= (0xFFU << (8 - trailing_bits));
}

std::ostream& operator<<(std::ostream& os, const bitfield& rhs)
{
    os << rhs.size() << '[';
    if (rhs)
    {
        boost::algorithm::hex(rhs.data(), rhs.data() + rhs.byte_size(),
                              std::ostream_iterator<char>(os));
    }
    os << ']';
    return os;
}

} // namespace bt_utils
} // namespace x3me
