#pragma once

#include "agg_meta_entry.h"

namespace cache
{
namespace detail
{

class memory_reader;
class memory_writer;
class range;

class agg_write_meta
{
public:
    using entries_t = std::vector<agg_meta_entry>;

private:
    using count_t = uint32_t;

    static constexpr uint64_t hdr_ftr_magic = 0xDEADBED01DEBDAED;

    // We use sorted vector here (like boost::container::flat_set).
    // We don't use the boost one, because with the vector we can
    // do faster serialization/deserialization, just memcpy.
    entries_t entries_;
    count_t max_cnt_entries_;

public:
    using const_iterator = entries_t::const_iterator;
    using size_type      = entries_t::size_type;

public:
    explicit agg_write_meta(bytes32_t meta_buff_size) noexcept;
    ~agg_write_meta() noexcept;

    bool load(memory_reader& reader) noexcept;
    void save(memory_writer& writer) noexcept;

    enum struct add_res
    {
        ok,
        overlaps,
        no_space,
    };
    // The method guarantees that it won't insert element which range overlaps
    // with already existing element. We need to prevent overlapping because
    // readers may get data directly from the aggregation buffer.
    add_res add_entry(const fs_node_key_t& key, const range_elem& rng) noexcept;
    const_iterator rem_entry(const_iterator it) noexcept;

    bool has_entry(const fs_node_key_t& key, const range_elem& rng) const
        noexcept;

    void set_entries(entries_t&& entries) noexcept;
    entries_t release_entries() noexcept;

    void clear() noexcept { return entries_.clear(); }

    const_iterator begin() const noexcept { return entries_.begin(); }
    const_iterator end() const noexcept { return entries_.end(); }

    bool empty() const noexcept { return entries_.empty(); }

    size_type cnt_entries() const noexcept { return entries_.size(); }

    size_type max_cnt_entries() const noexcept { return max_cnt_entries_; }
};

} // namespace detail
} // namespace cache
