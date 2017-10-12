#pragma once

#include "cache_common.h"

namespace cache
{
namespace detail
{

// Type safe unit for blocks. This can prevent some miscalculations
// if we were using plain number type.
// However it's very far from the safety provided by things like boost::unit.
// TODO Move the implementation to the cpp file, and implicitly instantiate
// for the needed types and block sizes.
template <typename NumType, uint32_t BlockSize>
class unit_blocks
{
    static_assert(x3me::math::is_pow_of_2(BlockSize), "");
    static_assert(std::is_unsigned<NumType>::value,
                  "Works only with unsigned integers");

    NumType cnt_;

public:
    enum : uint64_t
    {
        block_size = BlockSize
    };

public:
    static constexpr unit_blocks zero() noexcept
    {
        unit_blocks res{};
        res.cnt_ = 0;
        return res;
    }

    static constexpr unit_blocks create_from_bytes(uint64_t bytes) noexcept
    {
        unit_blocks res{};
        res.set_from_bytes(bytes);
        return res;
    }

    static constexpr unit_blocks round_up_to_blocks(uint64_t bytes) noexcept
    {
        using x3me::math::round_up_pow2;
        unit_blocks res{};
        res.set_from_bytes(round_up_pow2(bytes, block_size));
        return res;
    }

    static constexpr unit_blocks round_down_to_blocks(uint64_t bytes) noexcept
    {
        unit_blocks res{};
        res.set_from_bytes((bytes / block_size) * block_size);
        return res;
    }

    static constexpr unit_blocks create_from_blocks(NumType blocks) noexcept
    {
        unit_blocks res{};
        res.set_from_blocks(blocks);
        return res;
    }

    template <typename T>
    static constexpr unit_blocks copy(unit_blocks<T, BlockSize> rhs) noexcept
    {
        // This check should work well because of the is_unsigned check above
        static_assert(sizeof(T) <= sizeof(NumType),
                      "Narrowing conversions are not allowed");
        unit_blocks res{};
        res.cnt_ = rhs.value();
        return res;
    }

    constexpr void set_from_bytes(uint64_t bytes) noexcept
    {
        assert((bytes % block_size) == 0 &&
               "The bytes value must be modulo of the block_size");
        cnt_ = bytes / block_size;
    }

    constexpr void set_from_blocks(NumType blocks) noexcept { cnt_ = blocks; }

    constexpr NumType value() const noexcept { return cnt_; }
    constexpr bytes64_t to_bytes() const noexcept
    {
        return static_cast<uint64_t>(cnt_) * block_size;
    }

    unit_blocks& operator+=(const unit_blocks& rhs) noexcept
    {
        cnt_ += rhs.cnt_;
        return *this;
    }
    unit_blocks& operator-=(const unit_blocks& rhs) noexcept
    {
        cnt_ -= rhs.cnt_;
        return *this;
    }

    unit_blocks& operator|=(const unit_blocks& rhs) noexcept
    {
        cnt_ |= rhs.cnt_;
        return *this;
    }

    unit_blocks& operator<<=(uint8_t num) noexcept
    {
        cnt_ <<= num;
        return *this;
    }
};

template <typename T, uint32_t S>
std::ostream& operator<<(std::ostream& os,
                         const unit_blocks<T, S>& rhs) noexcept
{
    return os << rhs.value();
}

////////////////////////////////////////////////////////////////////////////////
// Operations

template <typename NumType, uint32_t BlockSize>
bool operator==(const unit_blocks<NumType, BlockSize>& lhs,
                const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    return (lhs.value() == rhs.value());
}

template <typename NumType, uint32_t BlockSize>
bool operator!=(const unit_blocks<NumType, BlockSize>& lhs,
                const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    return (lhs.value() != rhs.value());
}

template <typename NumType, uint32_t BlockSize>
bool operator<(const unit_blocks<NumType, BlockSize>& lhs,
               const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    return (lhs.value() < rhs.value());
}

template <typename NumType, uint32_t BlockSize>
bool operator<=(const unit_blocks<NumType, BlockSize>& lhs,
                const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    return (lhs.value() <= rhs.value());
}

template <typename NumType, uint32_t BlockSize>
bool operator>(const unit_blocks<NumType, BlockSize>& lhs,
               const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    return (lhs.value() > rhs.value());
}

template <typename NumType, uint32_t BlockSize>
bool operator>=(const unit_blocks<NumType, BlockSize>& lhs,
                const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    return (lhs.value() >= rhs.value());
}

template <typename NumType, uint32_t BlockSize>
unit_blocks<NumType, BlockSize>
operator+(const unit_blocks<NumType, BlockSize>& lhs,
          const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    auto res = lhs;
    res += rhs;
    return res;
}

template <typename NumType, uint32_t BlockSize>
unit_blocks<NumType, BlockSize>
operator-(const unit_blocks<NumType, BlockSize>& lhs,
          const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    auto res = lhs;
    res -= rhs;
    return res;
}

template <typename NumType, uint32_t BlockSize>
unit_blocks<NumType, BlockSize>
operator|(const unit_blocks<NumType, BlockSize>& lhs,
          const unit_blocks<NumType, BlockSize>& rhs) noexcept
{
    auto res = lhs;
    res |= rhs;
    return res;
}

template <typename NumType, uint32_t BlockSize>
unit_blocks<NumType, BlockSize>
operator<<(const unit_blocks<NumType, BlockSize>& lhs, uint8_t rhs) noexcept
{
    auto res = lhs;
    res <<= rhs;
    return res;
}

////////////////////////////////////////////////////////////////////////////////

using volume_blocks8_t  = unit_blocks<uint8_t, volume_block_size>;
using volume_blocks16_t = unit_blocks<uint16_t, volume_block_size>;
using volume_blocks32_t = unit_blocks<uint32_t, volume_block_size>;
using volume_blocks64_t = unit_blocks<uint64_t, volume_block_size>;
using store_blocks32_t  = unit_blocks<uint32_t, store_block_size>;
using store_blocks64_t  = unit_blocks<uint64_t, store_block_size>;

static_assert(
    std::is_pod<volume_blocks8_t>::value,
    "Needed for memcpy, memset, etc. Although is_trivial should suffice");
static_assert(
    std::is_pod<volume_blocks16_t>::value,
    "Needed for memcpy, memset, etc. Although is_trivial should suffice");
static_assert(
    std::is_pod<volume_blocks32_t>::value,
    "Needed for memcpy, memset, etc. Although is_trivial should suffice");
static_assert(
    std::is_pod<volume_blocks64_t>::value,
    "Needed for memcpy, memset, etc. Although is_trivial should suffice");
static_assert(
    std::is_pod<store_blocks32_t>::value,
    "Needed for memcpy, memset, etc. Although is_trivial should suffice");
static_assert(
    std::is_pod<store_blocks64_t>::value,
    "Needed for memcpy, memset, etc. Although is_trivial should suffice");

} // namespace detail
} // namespace cache
