#pragma once

#include "fs_metadata_hdr.h"
#include "fs_ops_data.h"
#include "fs_table.h"
#include "fs_metadata_ftr.h"

namespace cache
{
struct stats_fs_md;
struct stats_fs_ops;
namespace detail
{

class volume_info;
class disk_reader;
class memory_writer;
class object_key;
class range;
class write_transaction;
class agg_meta_entry;

class fs_metadata
{
    fs_metadata_hdr hdr_;
    fs_ops_data ops_;
    fs_table table_;
    fs_metadata_ftr ftr_;

    bool is_dirty_ = false;

public:
    fs_metadata(const volume_info& vi, bytes32_t min_avg_obj_size) noexcept;
    ~fs_metadata() noexcept;

    fs_metadata(const fs_metadata& rhs) noexcept;

    fs_metadata(fs_metadata&& rhs) noexcept;
    fs_metadata& operator=(fs_metadata&& rhs) noexcept;

    fs_metadata& operator=(const fs_metadata&) = delete;

    void clean_init(bytes64_t init_data_offs) noexcept;

    // Returns true if the metadata is successfully loaded.
    // Returns false if the metadata is invalid and can't be loaded.
    // Throws in case of IO error.
    bool load(disk_reader& reader);

    void save(memory_writer& writer) const noexcept;

    bytes64_t size_on_disk() const noexcept;
    bytes64_t max_size_on_disk() const noexcept;

    template <typename OverwriteCond>
    fs_table::add_res add_table_entry(const fs_node_key_t& key,
                                      const range_elem& rng,
                                      OverwriteCond&& overwrite) noexcept;
    // Returns the count of the removed ranges, if the key is found
    template <typename Remover>
    optional_t<uint32_t> rem_table_entries(const fs_node_key_t& key,
                                           Remover&& rem) noexcept;
    bool rem_table_entry(const fs_node_key_t& key,
                         const range_elem& rng) noexcept;
    template <typename Reader>
    bool read_table_entries(const fs_node_key_t& key, Reader&& rdr) const
        noexcept;
    template <typename Modifier>
    bool modify_table_entries(const fs_node_key_t& key,
                              Modifier&& mod) noexcept;

    void inc_sync_serial() noexcept;
    void dec_sync_serial() noexcept;
    uint32_t sync_serial() const noexcept { return hdr_.sync_serial(); }

    const uuid_t& uuid() const noexcept { return hdr_.uuid(); }

    bytes64_t write_pos() const noexcept { return ops_.write_pos(); }
    uint64_t write_lap() const noexcept { return ops_.write_lap(); }

    void inc_write_pos(bytes64_t pos) noexcept;
    void wrap_write_pos(bytes64_t init_write_pos) noexcept;

#ifdef X3ME_TEST
    void set_write_pos(bytes64_t write_pos, uint64_t write_lap) noexcept;
#endif

    void set_non_dirty() noexcept { is_dirty_ = false; }
    bool is_dirty() const noexcept { return is_dirty_; }

    void get_stats(stats_fs_md& smd, stats_fs_ops& sops) const noexcept;

private:
    // The function returns 0 or 1 if the metadata is correct.
    // The returned number corresponds to the copy of valid metadata which
    // should be used. It returns -1 if there is no valid metadata found.
    // The function throws in case of IO error.
    int load_check_metadata_hdr_ftr(disk_reader& reader);
    // Returns true if the loaded metadata is correct.
    // Returns false if the loaded metadata is corrupted/wrong.
    // Throws in case of IO error.
    bool load_full_metadata(disk_reader& reader, uint32_t metadata_idx);

    friend std::ostream& operator<<(std::ostream& os,
                                    const fs_metadata& rhs) noexcept;
};

////////////////////////////////////////////////////////////////////////////////

template <typename OverwriteCond>
fs_table::add_res
fs_metadata::add_table_entry(const fs_node_key_t& key,
                             const range_elem& rng,
                             OverwriteCond&& overwrite) noexcept
{
    is_dirty_ = true; // Add entry may fail/be skipped, but ...
    return table_.add_entry(key, rng, std::forward<OverwriteCond>(overwrite));
}

template <typename Remover>
optional_t<uint32_t> fs_metadata::rem_table_entries(const fs_node_key_t& key,
                                                    Remover&& rem) noexcept
{
    is_dirty_ = true; // Remove entries may actually don't remove any, but ...
    return table_.rem_entries(key, std::forward<Remover>(rem));
}

template <typename Reader>
bool fs_metadata::read_table_entries(const fs_node_key_t& key,
                                     Reader&& rdr) const noexcept
{
    return table_.read_entries(key, std::forward<Reader>(rdr));
}

template <typename Modifier>
bool fs_metadata::modify_table_entries(const fs_node_key_t& key,
                                       Modifier&& mod) noexcept
{
    // When we modify entries we modify their metadata and we don't want this
    // to provoke flush on the disk. Thus, we don't set the dirty flag here.
    return table_.modify_entries(key, std::forward<Modifier>(mod));
}

} // namespace detail
} // namespace cache
