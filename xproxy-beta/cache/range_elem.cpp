#include "precompiled.h"
#include "range_elem.h"

namespace cache
{
namespace detail
{

bytes64_t range_elem::rng_offset() const noexcept
{
    static_assert(sizeof(rng_offset_lo_) == 4, "Fix the calculations below");
    static_assert(sizeof(rng_offset_hi_) == 1, "Fix the calculations below");
    return (bytes64_t(rng_offset_hi_) << 32) | rng_offset_lo_;
}

bytes64_t range_elem::rng_end_offset() const noexcept
{
    return rng_offset() + rng_size();
}

bytes32_t range_elem::rng_size() const noexcept
{
    static_assert(sizeof(rng_size_lo_) == 2, "Fix the calculations below");
    static_assert(sizeof(rng_size_hi_) == 1, "Fix the calculations below");
    return (bytes64_t(rng_size_hi_) << 16) | rng_size_lo_;
}

volume_blocks64_t range_elem::disk_offset() const noexcept
{
    static_assert(sizeof(disk_offset_lo_) == 4, "Fix the calculations below");
    static_assert(sizeof(disk_offset_hi_) == 1, "Fix the calculations below");
    const auto offs_hi = volume_blocks64_t::copy(disk_offset_hi_);
    const auto offs_lo = volume_blocks64_t::copy(disk_offset_lo_);
    return (offs_hi << 32) | offs_lo;
}

volume_blocks64_t range_elem::disk_end_offset() const noexcept
{
    return disk_offset() + volume_blocks64_t::round_up_to_blocks(rng_size());
}

void range_elem::set_rng_offset(bytes64_t v) noexcept
{
    static_assert(sizeof(rng_offset_lo_) == 4, "");
    static_assert(sizeof(rng_offset_hi_) == 1, "");
    X3ME_ENFORCE(v < max_obj_size, "Unsupported range offset. Too big");
    rng_offset_lo_ = v & 0xFFFFFFFF;
    rng_offset_hi_ = (v >> 32) & 0xFF;
}

void range_elem::set_rng_size(bytes32_t v) noexcept
{
    static_assert(sizeof(rng_size_lo_) == 2, "");
    static_assert(sizeof(rng_size_hi_) == 1, "");
    X3ME_ENFORCE(x3me::math::in_range(v, min_rng_size(), max_rng_size() + 1),
                 "Unsupported range size. Too small or too big");
    rng_size_lo_ = v & 0xFFFF;
    rng_size_hi_ = (v >> 16) & 0xFF;
}

void range_elem::set_disk_offset(volume_blocks64_t v) noexcept
{
    static_assert(sizeof(disk_offset_lo_) == 4, "");
    static_assert(sizeof(disk_offset_hi_) == 1, "");
    X3ME_ENFORCE(
        x3me::math::in_range(v.to_bytes(), volume_skip_bytes, max_volume_size),
        "Unsupported disk offset. Too small or too big");
    disk_offset_lo_ =
        volume_blocks32_t::create_from_blocks(v.value() & 0xFFFFFFFF);
    disk_offset_hi_ =
        volume_blocks8_t::create_from_blocks((v.value() >> 32) & 0xFF);
}

////////////////////////////////////////////////////////////////////////////////

range_elem make_range_elem(bytes64_t rng_beg,
                           bytes32_t rng_len,
                           volume_blocks64_t disk_offs) noexcept
{
    range_elem ret;
    ret.reset_meta();
    ret.set_rng_offset(rng_beg);
    ret.set_rng_size(rng_len);
    ret.set_disk_offset(disk_offs);
    return ret;
}

range_elem make_zero_range_elem() noexcept
{
    range_elem ret;
    static_assert(std::is_pod<range_elem>::value, "Needed for memset below");
    ::memset(&ret, 0, sizeof(ret));
    ret.reset_meta();
    return ret;
}

bool valid_range_elem(const range_elem& rng,
                      bytes64_t disk_offs,
                      bytes64_t disk_size) noexcept
{
    return range::is_valid(rng.rng_offset(), rng.rng_size(), frag_rng) &&
           x3me::math::in_range(rng.disk_offset().to_bytes(),
                                rng.disk_end_offset().to_bytes(), disk_offs,
                                disk_offs + disk_size);
}

std::ostream& operator<<(std::ostream& os, const range_elem& rhs) noexcept
{
    // clang-format off
    return os << '{' << rhs.rng_offset() 
              << ',' << rhs.rng_size()
              << ',' << rhs.disk_offset()
              << ',' << static_cast<uint16_t>(rhs.cnt_readers()) << '}';
    // clang-format on
}
} // namespace detail
} // namespace cache
