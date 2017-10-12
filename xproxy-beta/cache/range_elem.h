#pragma once

#include "cache_common.h"
#include "range.h"
#include "unit_blocks.h"

namespace cache
{
namespace detail
{

class range_elem
{
public:
    enum : uint8_t
    {
        elem_mark = 0x00,
    };

private:
    // Note: It's important that the mark is the first field because the
    // range_vector functionality uses SBO and constructs one range_elem
    // in place. It distinguishes if it has single in-place range_elem or
    // heap allocated range elements by the first byte.
    uint8_t mark_;
    // This member is used through atomic operations.
    // However, we use it through the compiler built-ins atomic functions
    // because the std::atomic variant disables the automatically generated
    // copy constructor and adding one by hand makes the class non-POD.
    // Note that we don't need thread safety of the copy copy constructor.
    uint8_t cnt_readers_;
    bytes16_t rng_size_lo_;
    bytes32_t rng_offset_lo_;
    volume_blocks32_t disk_offset_lo_;
    bytes8_t rng_size_hi_;
    bytes8_t rng_offset_hi_;
    volume_blocks8_t disk_offset_hi_;
    uint8_t in_memory_; // Used as bool. 7 bytes can be used for more readers

public:
    static constexpr auto members_size() noexcept
    {
        // NOTE Keep this function in sync with the above members,
        // otherwise bad things may happen :).
        // clang-format off
        return sizeof(mark_) + 
               sizeof(cnt_readers_) +
               sizeof(rng_size_lo_) + 
               sizeof(rng_offset_lo_) +
               sizeof(disk_offset_lo_) + 
               sizeof(rng_size_hi_) +
               sizeof(rng_offset_hi_) + 
               sizeof(disk_offset_hi_) +
               sizeof(in_memory_);
        // clang-format on
    }

    static constexpr bytes64_t max_supported_rng_offset() noexcept
    {
        auto byte_size = sizeof(rng_offset_lo_) + sizeof(rng_offset_hi_);
        return (1ULL << (byte_size * CHAR_BIT)) - 1;
    }
    static constexpr bytes32_t max_supported_rng_size() noexcept
    {
        auto byte_size = sizeof(rng_size_lo_) + sizeof(rng_size_hi_);
        return (1U << (byte_size * CHAR_BIT)) - 1;
    }
    static constexpr bytes64_t max_supported_disk_offset() noexcept
    {
        auto block_size = sizeof(disk_offset_lo_) + sizeof(disk_offset_hi_);
        return ((1ULL << (block_size * CHAR_BIT)) - 1) * volume_block_size;
    }

    static constexpr bytes32_t min_rng_size() noexcept
    {
        return object_frag_min_data_size;
    }
    static constexpr bytes32_t max_rng_size() noexcept
    {
        return object_frag_max_data_size;
    }

    static constexpr auto max_cnt_readers() noexcept
    {
        return std::numeric_limits<decltype(cnt_readers_)>::max();
    }

public:
    bytes64_t rng_offset() const noexcept;
    bytes64_t rng_end_offset() const noexcept;
    bytes32_t rng_size() const noexcept;
    volume_blocks64_t disk_offset() const noexcept;
    volume_blocks64_t disk_end_offset() const noexcept;

    void set_rng_offset(bytes64_t v) noexcept;
    // Note that the size of a range element can't be more than
    // object fragment size, because we internally split ranges bigger than
    // the fragment size to separate fragments.
    void set_rng_size(bytes32_t v) noexcept;
    void set_disk_offset(volume_blocks64_t v) noexcept;

    void reset_meta() noexcept
    {
        mark_        = elem_mark;
        cnt_readers_ = 0;
        in_memory_   = 0;
    }

    void set_mark() noexcept { mark_ = elem_mark; }
    bool atomic_inc_readers() noexcept
    {
        constexpr auto maxc = max_cnt_readers();
        auto v              = cnt_readers();
        do
        {
            if (v == maxc)
                return false;
        } while (!__atomic_compare_exchange_n(&cnt_readers_, &v, v + 1,
                                              true /*weak*/, __ATOMIC_SEQ_CST,
                                              __ATOMIC_SEQ_CST));
        return true;
    }
    void atomic_dec_readers() noexcept
    {
        auto v = cnt_readers();
        while ((v > 0) &&
               !__atomic_compare_exchange_n(&cnt_readers_, &v, v - 1,
                                            true /*weak*/, __ATOMIC_SEQ_CST,
                                            __ATOMIC_SEQ_CST))
        {
        }
        X3ME_ENFORCE(v > 0, "A successful call to inc_readers must be paired "
                            "with a call to dec_readers");
    }
    auto cnt_readers() const noexcept -> decltype(cnt_readers_)
    {
        return __atomic_load_n(&cnt_readers_, __ATOMIC_ACQUIRE);
    }
    bool has_readers() const noexcept { return cnt_readers() > 0; }

    void set_in_memory(bool v) noexcept { in_memory_ = v; }
    bool in_memory() const noexcept { return in_memory_; }

    static bool is_range_elem(const void* mem) noexcept
    {
        return (*static_cast<const uint8_t*>(mem) == elem_mark);
    }
};
static_assert(std::is_pod<range_elem>::value, "Needed for memcpy, memset, etc");
static_assert(sizeof(range_elem) == 16, "");
static_assert(alignof(range_elem) == 4, "");
static_assert(range_elem::max_supported_rng_offset() >= max_obj_size,
              "Can't support so big objects");
static_assert(range_elem::max_supported_rng_size() >=
                  range_elem::max_rng_size(),
              "Can't support so big range elements");
static_assert(range_elem::max_supported_disk_offset() >=
                  (max_volume_size -
                   round_to_volume_block_size(object_frag_hdr_size +
                                              range_elem::min_rng_size())),
              "Can't support so big volumes");

////////////////////////////////////////////////////////////////////////////////
// These operators must be kept in sync
inline bool operator<(const range_elem& lhs, const range_elem& rhs) noexcept
{
    return lhs.rng_offset() < rhs.rng_offset();
}

inline bool operator<(const range_elem& lhs, const range& rhs) noexcept
{
    return lhs.rng_offset() < rhs.beg();
}

inline bool operator<(const range& lhs, const range_elem& rhs) noexcept
{
    return lhs.beg() < rhs.rng_offset();
}

inline bool operator==(const range_elem& lhs, const range_elem& rhs) noexcept
{
    return (lhs.rng_offset() == rhs.rng_offset()) &&
           (lhs.rng_size() == rhs.rng_size()) &&
           (lhs.disk_offset() == rhs.disk_offset());
}

////////////////////////////////////////////////////////////////////////////////

// Note that the max possible supported range length is limited by the
// fragment max size because we split bigger ranges in separate disk fragments.
range_elem make_range_elem(bytes64_t rng_beg, bytes32_t rng_len,
                           volume_blocks64_t disk_offs) noexcept;
range_elem make_zero_range_elem() noexcept;

bool valid_range_elem(const range_elem& rng, bytes64_t diks_offs,
                      bytes64_t disk_size) noexcept;

inline range to_range(const range_elem& rng) noexcept
{
    return range{rng.rng_offset(), rng.rng_size(), frag_rng};
}

std::ostream& operator<<(std::ostream& os, const range_elem& rhs) noexcept;

} // namespace detail
} // namespace cache
