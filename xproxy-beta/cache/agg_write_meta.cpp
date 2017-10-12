#include "precompiled.h"
#include "agg_write_meta.h"
#include "memory_writer.h"
#include "memory_reader.h"

namespace cache
{
namespace detail
{

agg_write_meta::agg_write_meta(bytes32_t meta_buff_size) noexcept
    : max_cnt_entries_((meta_buff_size / sizeof(agg_meta_entry)) - 1)
{
    static_assert(
        sizeof(agg_meta_entry) >=
            (sizeof(hdr_ftr_magic) + sizeof(count_t) + sizeof(hdr_ftr_magic)),
        "We need to use the space for single entry to write the "
        "header, the number of entries and the footer");
    X3ME_ENFORCE(meta_buff_size > (2 * sizeof(agg_meta_entry)),
                 "The size of the provided buffer is too small");
}

agg_write_meta::~agg_write_meta() noexcept
{
}

bool agg_write_meta::load(memory_reader& reader) noexcept
{
    auto magic = hdr_ftr_magic;

    // Header magic
    reader.read(&magic, sizeof(magic));
    if (magic != hdr_ftr_magic)
        return false;

    auto cnt_entries = max_cnt_entries_;
    reader.read(&cnt_entries, sizeof(cnt_entries));
    if (cnt_entries > max_cnt_entries_)
        return false;

    static_assert(std::is_trivial<agg_meta_entry>::value, "Needed for memcpy");
    entries_t entries(cnt_entries);
    reader.read(entries.data(), cnt_entries * sizeof(agg_meta_entry));

    if (!std::is_sorted(entries.cbegin(), entries.cend()))
        return false;

    // Footer magic
    reader.read(&magic, sizeof(magic));
    if (magic != hdr_ftr_magic)
        return false;

    entries_.swap(entries);

    return true;
}

void agg_write_meta::save(memory_writer& writer) noexcept
{
    const auto magic = hdr_ftr_magic;

    // Header magic
    writer.write(&magic, sizeof(magic));

    count_t cnt_entries = entries_.size();
    writer.write(&cnt_entries, sizeof(cnt_entries));

    static_assert(std::is_trivial<agg_meta_entry>::value, "Needed for memcpy");
    writer.write(entries_.data(), cnt_entries * sizeof(agg_meta_entry));

    // Footer magic
    writer.write(&magic, sizeof(magic));
}

agg_write_meta::add_res
agg_write_meta::add_entry(const fs_node_key_t& key,
                          const range_elem& rng) noexcept
{
    add_res ret =
        (entries_.size() < max_cnt_entries_) ? add_res::ok : add_res::no_space;
    if (ret == add_res::ok)
    {
        auto overlap = [](const auto& lhs, const auto& rhs)
        {
            return (lhs.key() == rhs.key()) &&
                   (x3me::math::ranges_overlap(lhs.rng().rng_offset(),
                                               lhs.rng().rng_end_offset(),
                                               rhs.rng().rng_offset(),
                                               rhs.rng().rng_end_offset()) > 0);
        };
        const agg_meta_entry e{key, rng};
        auto it = std::lower_bound(entries_.begin(), entries_.end(), e);
        if (it == entries_.begin())
        {
            if (entries_.empty() || !overlap(*it, e)) // If it overlaps next one
                entries_.insert(it, e);
            else
                ret = add_res::overlaps;
        }
        else if (it == entries_.end())
        {
            X3ME_ASSERT(!entries_.empty(),
                        "We must have entered previous case");
            auto prev = it - 1;
            if (!overlap(*prev, e)) // If it overlaps previous one
                entries_.insert(it, e);
            else
                ret = add_res::overlaps;
        }
        else
        {
            auto prev = it - 1;
            // Check if it overlaps with the prev and next entries.
            if (!overlap(*prev, e) && !overlap(*it, e))
                entries_.insert(it, e);
            else
                ret = add_res::overlaps;
        }
    }
    return ret;
}

agg_write_meta::const_iterator
agg_write_meta::rem_entry(const_iterator it) noexcept
{
    return entries_.erase(it);
}

bool agg_write_meta::has_entry(const fs_node_key_t& key,
                               const range_elem& rng) const noexcept
{
    const agg_meta_entry_view e{key, rng};
    auto it = std::lower_bound(entries_.begin(), entries_.end(), e);
    return (it != entries_.end()) && (*it == e);
}

void agg_write_meta::set_entries(entries_t&& entries) noexcept
{
    std::sort(entries.begin(), entries.end());
    entries_ = std::move(entries);
}

agg_write_meta::entries_t agg_write_meta::release_entries() noexcept
{
    entries_t ret;
    entries_.swap(ret);
    return ret;
}

} // namespace detail
} // namespace cache
