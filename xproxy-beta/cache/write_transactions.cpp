#include "precompiled.h"
#include "write_transactions.h"
#include "write_transaction.h"

namespace cache
{
namespace detail
{

write_transaction write_transactions::add_entry(const fs_node_key_t& key,
                                                range rng) noexcept
{
    X3ME_ASSERT(rng.empty(), "The given range must not be empty");

    // First remove the overlapping with already existing range_elements
    auto it = data_.find(key);
    if (it != data_.end())
        rng = it->second.trim_overlaps(rng);
    else
        it = data_.emplace(key, range_vector{}).first;

    constexpr bytes64_t rmin = range_elem::min_rng_size();
    constexpr bytes64_t rmax = range_elem::max_rng_size();
    constexpr auto no_disk_off =
        volume_blocks64_t::create_from_bytes(volume_skip_bytes);

    // Split the range to range_elements and try to add as much as possible.
    // Stop on the first entry which can't be added because it overlaps with
    // already existing one.
    auto& rng_vec = it->second;
    auto roff = rng.beg();
    for (const auto end_off = rng.end(); roff < end_off;)
    {
        const auto rlen = std::min(end_off - roff, rmax);
        if (rlen < rmin)
            break; // Must not add so small ranges
        auto ret = rng_vec.add_range(make_range_elem(roff, rlen, no_disk_off));
        if (ret.second)
            roff += rlen;
        else
            break;
    }

    X3ME_ASSERT(!rng_vec.empty(),
                "If we hit this assert we either have bug in the "
                "range_vector::trim_overlaps or non-empty, but invalid, range "
                "has been passed to the function");

    if (roff == rng.beg())
    {
        write_transaction wtrans{key, range{}};
        wtrans.invalidate();
        return wtrans;
    }
    return write_transaction{key, range{rng.beg(), roff - rng.beg()}};
}

void write_transactions::rem_entry(const write_transaction& wtrans) noexcept
{
    X3ME_ASSERT(wtrans.valid(), "Invalid transaction must not be passed");
    auto it = data_.find(wtrans.fs_node_key());
    X3ME_ASSERT((it != data_.end()), "The transaction must be present");
    if (it != data_.end())
    {
        const auto rng = wtrans.get_range();
        auto& rng_vec  = it->second;
        auto ret = rng_vec.find_exact_range(rng);
        X3ME_ASSERT(!ret.empty(), "The transaction range must be present");
        if (!ret.empty())
        {
            rng_vec.rem_range(ret);
            if (rng_vec.empty())
                data_.erase(it);
        }
    }
}

} // namespace detail
} // namespace cache
