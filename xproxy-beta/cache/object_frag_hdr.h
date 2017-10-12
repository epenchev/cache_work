#pragma once

#include "cache_common.h"
#include "fs_node_key.h"
#include "range_elem.h"

namespace cache
{
namespace detail
{

class object_frag_hdr
{
    uLong check_sum_;

    template <typename Num>
    static uLong csum(uLong sum, Num nm) noexcept
    {
        return adler32(sum, reinterpret_cast<const uint8_t*>(&nm), sizeof(nm));
    }

public:
    // Currently the checksum is calculated over the fragment key,
    // we could change this in the future if needed and the checksum
    // to be calculated, in the more expensive but correct way,
    // over the fragment data.
    static auto create(const fs_node_key_t& key, const range_elem& rng) noexcept
    {
        static_assert(sizeof(range_elem) == range_elem::members_size(),
                      "The range_elem must not have holes/paddings. See below");
        object_frag_hdr ret;
        ret.check_sum_ = adler32(0, nullptr, 0);
        ret.check_sum_ = adler32(ret.check_sum_, key.data(), key.size());
        ret.check_sum_ = csum(ret.check_sum_, rng.rng_offset());
        ret.check_sum_ = csum(ret.check_sum_, rng.rng_size());
        ret.check_sum_ = csum(ret.check_sum_, rng.disk_offset());
        return ret;
    }

    friend bool operator==(const object_frag_hdr& lhs,
                           const object_frag_hdr& rhs) noexcept
    {
        return lhs.check_sum_ == rhs.check_sum_;
    }

    friend bool operator!=(const object_frag_hdr& lhs,
                           const object_frag_hdr& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const object_frag_hdr& rhs) noexcept
    {
        return os << rhs.check_sum_;
    }
};
static_assert(std::is_pod<object_frag_hdr>::value,
              "Needed for memcpy, memset, etc");
static_assert(sizeof(object_frag_hdr) == object_frag_hdr_size,
              "Correct the constant!!!");

constexpr inline bytes32_t object_frag_size(bytes32_t data_size) noexcept
{
    return round_to_volume_block_size(sizeof(object_frag_hdr) + data_size);
}

} // namespace detail
} // namespace cache
