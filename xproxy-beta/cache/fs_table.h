#pragma once

#include "range_vector.h"
#include "fs_node_key.h"

namespace cache
{
namespace detail
{

class disk_reader;
class memory_writer;

class fs_table
{
public:
    struct disk_hdr
    {
        static constexpr uint64_t magic = 0xFEEDCAFEDEADBEEF;

        uint64_t magic_;
        uint64_t cnt_nodes_;
        uint64_t cnt_ranges_;
        uint64_t cnt_entries_;
        bytes64_t table_data_size_; // The size of the table data
        bytes64_t entries_data_size_; // The size of all entries data
    };

private:
    struct fs_node_hash
    {
        size_t operator()(const fs_node_key_t& v) const noexcept;
    };
    using fs_nodes_t =
        google::sparse_hash_map<fs_node_key_t, range_vector, fs_node_hash>;
    using fs_node_t = fs_nodes_t::value_type;
    static_assert(sizeof(fs_node_t) ==
                      (sizeof(fs_node_key_t) + sizeof(range_vector)),
                  "");

    const bytes64_t max_allowed_data_size_;

    static_assert(range_vector::has_sbo(),
                  "Currently we don't count the "
                  "in-place ranges in the below "
                  "counter. We rely on the SBO here.");
    // Doesn't include ranges from single element range vectors
    uint64_t cnt_ranges_ = 0;
    // An entry is a unique pair of fs_node + range_elem.
    uint64_t cnt_entries_ = 0;
    // The sum size of all entries data
    bytes64_t entries_data_size_ = 0;

    fs_nodes_t fs_nodes_;

public:
    fs_table(bytes64_t avail_disk_space, bytes32_t min_avg_obj_size) noexcept;
    ~fs_table() noexcept;

    fs_table(const fs_table& rhs) noexcept;

    fs_table(fs_table&& rhs) noexcept;
    fs_table& operator=(fs_table&& rhs) noexcept;

    fs_table& operator=(const fs_table&) = delete;

    void clean_init() noexcept;

    using err_info_t = x3me::utils::string_builder_256;
    // Returns true if the table is successfully loaded.
    // Returns false if the table is invalid and can't be loaded.
    // Throws in case of IO error.
    bool load(disk_reader& reader, err_info_t& out_err);

    void save(memory_writer& writer) const noexcept;

    // TODO Using function_ref would allow to remove the template-ness by
    // functor
    enum struct add_res
    {
        added,
        overwrote,
        skipped,
        limit_reached,
    };
    template <typename OverwriteCond>
    add_res add_entry(const fs_node_key_t& key,
                      const range_elem& rng,
                      OverwriteCond&& overwrite) noexcept;

    // Returns the count of the removed ranges if the given key is found
    template <typename Remover>
    optional_t<uint32_t> rem_entries(const fs_node_key_t& key,
                                     Remover&& rem) noexcept;

    template <typename Reader>
    bool read_entries(const fs_node_key_t& key, Reader&& rdr) const noexcept;

    // This function modify the range elements for a given key.
    // However, it's not allowed to add or remove range elements only to
    // modify the metadata associated with them.
    template <typename Modifier>
    bool modify_entries(const fs_node_key_t& key, Modifier&& mod) noexcept;

    bytes64_t size_on_disk() const noexcept;
    bytes64_t max_size_on_disk() const noexcept;

    auto max_allowed_data_size() const noexcept
    {
        return max_allowed_data_size_;
    }
    auto entries_data_size() const noexcept { return entries_data_size_; }
    auto cnt_entries() const noexcept { return cnt_entries_; }
    auto cnt_fs_nodes() const noexcept { return fs_nodes_.size(); }
    auto cnt_ranges() const noexcept { return cnt_ranges_; }

    bool limit_reached() const noexcept;

    static bool load(disk_reader& reader, disk_hdr& hdr, err_info_t& out_err);

    static bytes64_t data_size(uint64_t cnt_fs_nodes,
                               uint64_t cnt_ranges) noexcept;
    static bytes64_t full_size(bytes64_t data_size) noexcept;
    static bytes64_t max_full_size(bytes64_t disk_space,
                                   bytes32_t min_object_size) noexcept;

private:
    void on_inc_entries(const range_elem& rng) noexcept;
    void on_dec_entries(uint64_t cnt_removed, bytes64_t rem_size) noexcept;
    void on_dec_entries(const range_elem& rng) noexcept;
    void on_dec_entries(const range_vector::iter_range& rngs) noexcept;

    static bytes64_t max_data_size(bytes64_t disk_space,
                                   bytes32_t min_object_size) noexcept;

    // The entries are always added one by one, but several entries could be
    // removed at once.
    static uint32_t calc_inc_cnt_ranges(uint32_t rv_size) noexcept;
    static uint32_t calc_dec_cnt_ranges(uint32_t rv_size,
                                        uint32_t dec) noexcept;

    friend std::ostream& operator<<(std::ostream& os,
                                    const fs_table& rhs) noexcept;
};

std::ostream& operator<<(std::ostream& os,
                         const fs_table::add_res& rhs) noexcept;

////////////////////////////////////////////////////////////////////////////////
// TODO The functors here can be changed with function_ref when available

template <typename OverwriteCond>
fs_table::add_res fs_table::add_entry(const fs_node_key_t& key,
                                      const range_elem& rng,
                                      OverwriteCond&& overwrite) noexcept
{
    auto key_it = fs_nodes_.find(key);

    if (key_it == fs_nodes_.end())
    {
        if (data_size(fs_nodes_.size() + 1, cnt_ranges_) >
            max_allowed_data_size_)
            return add_res::limit_reached;

        // We don't count the inplace range_elements.
        // Thus we don't increase the cnt_ranges_ here.
        fs_nodes_.insert(fs_node_t(key, range_vector(rng)));
        on_inc_entries(rng);
        return add_res::added;
    }

    range_vector& rvec = key_it->second;
    X3ME_ENFORCE(!rvec.empty(), "We must not keep empty entries");

    const auto rngs = rvec.find_in_range(to_range(rng));
    if (rngs.empty())
    {
        const auto inc = calc_inc_cnt_ranges(rvec.size());
        if (data_size(fs_nodes_.size(), cnt_ranges_ + inc) >
            max_allowed_data_size_)
            return add_res::limit_reached;

        // If this fail the limit of the vector has been reached.
        // We'll count it as skipped.
        if (rvec.add_range(rng).second)
        {
            cnt_ranges_ += inc;
            on_inc_entries(rng);
            return add_res::added;
        }
    }
    else if (overwrite(rngs, rng))
    {
        const auto cnt_before = rvec.size();
        on_dec_entries(rngs);
        rvec.rem_range(rngs);
        const auto cnt_now = rvec.size();

        const auto dec = calc_dec_cnt_ranges(cnt_before, cnt_before - cnt_now);
        X3ME_ENFORCE(cnt_ranges_ >= dec, "Wrong logic for ranges counting");
        cnt_ranges_ -= dec;

        const auto inc = calc_inc_cnt_ranges(cnt_now);
        X3ME_ENFORCE(dec >= inc, "Wrong logic for inc/dec calculations");

        const auto r = rvec.add_range(rng).second;
        X3ME_ASSERT(r,
                    "Insert must succeed. Overlapped ranges has been removed");
        cnt_ranges_ += inc;
        on_inc_entries(rng);
        return add_res::overwrote;
    }
    return add_res::skipped;
}

template <typename Remover>
optional_t<uint32_t> fs_table::rem_entries(const fs_node_key_t& key,
                                           Remover&& rem) noexcept
{
    optional_t<uint32_t> ret;

    auto it = fs_nodes_.find(key);
    if (it == fs_nodes_.end())
        return ret;

    auto& rvec            = it->second;
    const auto cnt_before = rvec.size();

    const auto rem_size = rem(rvec);

    const auto cnt_removed = cnt_before - rvec.size();
    const auto dec = calc_dec_cnt_ranges(cnt_before, cnt_removed);
    X3ME_ENFORCE(cnt_ranges_ >= dec, "Wrong logic for ranges counting");
    cnt_ranges_ -= dec;

    on_dec_entries(cnt_removed, rem_size);

    if (cnt_removed == cnt_before)
        fs_nodes_.erase(it); // All elements has been removed

    ret = cnt_removed;
    return ret;
}

template <typename Reader>
bool fs_table::read_entries(const fs_node_key_t& key, Reader&& rdr) const
    noexcept
{
    auto it = fs_nodes_.find(key);
    if (it == fs_nodes_.end())
        return false;

    const auto& rvec = it->second;

    rdr(rvec);

    return true;
}

template <typename Modifier>
bool fs_table::modify_entries(const fs_node_key_t& key, Modifier&& mod) noexcept
{
    auto it = fs_nodes_.find(key);
    if (it == fs_nodes_.end())
        return false;

    const auto& rvec = it->second;

    mod(rvec);

    return true;
}

} // namespace detail
} // namespace cache
