#pragma once

#include "cache_common.h"

namespace cache
{
namespace detail
{
struct frag_rng_t
{
};
constexpr frag_rng_t frag_rng{};

// TODO Currently this range can represent three different types depending
// on which constructor is called (empty range, object range and fragment range)
// . Just split it in order to increase the type safety.
// Also see the usage of range_vector::trim_overlaps which
// needs to return a range which is non empty but smaller than the max_obj_size.
class range
{
    bytes64_t beg_ = 0;
    bytes64_t len_ = 0;

public:
    static bool is_valid(bytes64_t beg, bytes64_t len, frag_rng_t) noexcept
    {
        using x3me::math::in_range;
        return in_range(len, (bytes64_t)object_frag_min_data_size,
                        object_frag_max_data_size + 1UL) &&
               in_range(beg, beg + len, 0UL, (bytes64_t)max_obj_size);
    }
    static bool is_valid(bytes64_t beg, bytes64_t len) noexcept
    {
        return (len >= min_obj_size) &&
               x3me::math::in_range(beg, beg + len, 0UL,
                                    (bytes64_t)max_obj_size);
    }

public:
    range() noexcept = default;
    range(bytes64_t beg, bytes64_t len, frag_rng_t) noexcept : beg_(beg),
                                                               len_(len)
    {
        X3ME_ENFORCE(is_valid(beg, len, frag_rng));
    }
    range(bytes64_t beg, bytes64_t len) noexcept : beg_(beg), len_(len)
    {
        X3ME_ENFORCE(is_valid(beg, len));
    }

    auto beg() const noexcept { return beg_; }
    auto end() const noexcept { return beg_ + len_; }
    auto len() const noexcept { return len_; }
    bool empty() const noexcept { return (len_ == 0); }
};

inline bool operator<(const range& lhs, const range& rhs) noexcept
{
    return lhs.beg() < rhs.beg();
}

inline bool operator==(const range& lhs, const range& rhs) noexcept
{
    return (lhs.beg() == rhs.beg()) && (lhs.len() == rhs.len());
}

inline std::ostream& operator<<(std::ostream& os, const range& rhs) noexcept
{
    return os << '[' << rhs.beg() << '-' << rhs.end() << ')';
}

} // namespace detail
} // namespace cache
