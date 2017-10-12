#pragma once

#include "cache_fs_ops.h"
#include "cache_fs_ops_fwds.h"
#include "unit_blocks.h"

namespace cache
{
struct stats_fs_md;
struct stats_fs_ops;
struct stats_internal;
namespace detail
{
class agg_writer;
class aio_service;
class range_vector;
class volume_fd;

class cache_fs_operations final : public cache_fs_ops
{
    using on_disk_error_cb_t = x3me::utils::mem_fn_delegate<void()>;

    // We use pointers for the objects which lives somewhere else, so that
    // we can easily unit test some of the functionality without a need to
    // instantiate every needed object outside.
    non_owner_ptr_t<volume_fd> fd_;
    non_owner_ptr_t<fs_metadata_sync_t> fs_meta_;
    non_owner_ptr_t<agg_writer> agg_writer_ = nullptr;
    non_owner_ptr_t<aio_service> aios_;
    non_owner_ptr_t<const boost::container::string> path_;

    x3me::thread::shared_mutex vol_mutex_;

    struct internal_stats
    {
        std::atomic<uint64_t> cnt_lock_volume_mtx_{0};
        std::atomic<uint64_t> cnt_no_lock_volume_mtx_{0};
        std::atomic<uint64_t> cnt_begin_write_ok_{0};
        std::atomic<uint64_t> cnt_begin_write_fail_{0};
        std::atomic<uint64_t> cnt_begin_write_trunc_ok_{0};
        std::atomic<uint64_t> cnt_begin_write_trunc_fail_{0};
        std::atomic<uint64_t> cnt_read_frag_mem_hit_{0};
        std::atomic<uint64_t> cnt_read_frag_mem_miss_{0};
        std::atomic<uint64_t> cnt_frag_meta_add_ok_{0};
        std::atomic<uint64_t> cnt_frag_meta_add_skipped_{0};
        std::atomic<uint32_t> cnt_frag_meta_add_limit_{0};
        std::atomic<uint32_t> cnt_frag_meta_add_overlaps_{0};
        std::atomic<uint32_t> cnt_readers_limit_reached_{0};
        std::atomic<uint32_t> cnt_failed_unmark_read_rng_{0};
        std::atomic<uint32_t> cnt_invalid_rng_elem_{0};
        std::atomic<uint32_t> cnt_evac_frag_no_mem_entry_{0};
    } internal_stats_;

    on_disk_error_cb_t on_disk_error_cb_;

    // The offset to the first data on the disk.
    const store_blocks64_t data_offset_;
    // The size, in blocks, which can be used for storing data.
    const store_blocks64_t cnt_data_blocks_;

public:
    cache_fs_operations(non_owner_ptr_t<volume_fd> fd,
                        non_owner_ptr_t<fs_metadata_sync_t> md,
                        non_owner_ptr_t<aio_service> aios,
                        non_owner_ptr_t<const boost::container::string> path,
                        store_blocks64_t data_offset,
                        store_blocks64_t cnt_data_blocks_) noexcept;
    ~cache_fs_operations() noexcept final;

    cache_fs_operations(const cache_fs_operations&) = delete;
    cache_fs_operations& operator=(const cache_fs_operations&) = delete;
    cache_fs_operations(cache_fs_operations&&) = delete;
    cache_fs_operations& operator=(cache_fs_operations&&) = delete;

    void set_on_disk_error_cb(const on_disk_error_cb_t& cb) noexcept;
    void set_agg_writer(non_owner_ptr_t<agg_writer> agw) noexcept;

    const boost::container::string& vol_path() const noexcept final;

    void report_disk_error() noexcept final;

    void get_stats(stats_fs_md& smd, stats_fs_ops& sops) const noexcept;
    void get_internal_stats(stats_internal& sts) const noexcept;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving the volume mutex
    bool vmtx_lock_shared(bytes64_t disk_offset) noexcept final;
    void vmtx_unlock_shared() noexcept final;
    void vmtx_wait_disk_readers() noexcept final;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving the aio_service
    void aios_push_read_queue(owner_ptr_t<aio_task> t) noexcept final;
    void aios_enqueue_read_queue(owner_ptr_t<aio_task> t) noexcept final;
    bool
    aios_cancel_task_read_queue(non_owner_ptr_t<aio_task> t) noexcept final;
    void aios_push_write_queue(owner_ptr_t<aio_task> t) noexcept final;
    void aios_enqueue_write_queue(owner_ptr_t<aio_task> t) noexcept final;
    void
    aios_push_front_write_queue(non_owner_ptr_t<agg_writer> t) noexcept final;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving fs_metadata
    read_transaction fsmd_begin_read(const object_key& key) noexcept final;
    void fsmd_end_read(read_transaction&& rtrans) noexcept final;
    expected_t<write_transaction, err_code_t>
    fsmd_begin_write(const object_key& key, bool truncate_obj) noexcept final;
    expected_t<range_elem, err_code_t>
    fsmd_find_next_range_elem(const read_transaction& rtrans) noexcept final;
    // The function removes the metadata for fragments which are not currently
    // read. These fragments don't need evacuation. It removes the metadata
    // from both the passed collection of fragments and from its memory
    // structures. This ensures that later arrived readers don't mess with the
    // current aggregate write which is going to overwrite the fragments.
    void fsmd_rem_non_evac_frags(std::vector<agg_meta_entry>&,
                                 volume_blocks64_t disk_offs,
                                 volume_blocks64_t area_size) noexcept final;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving fs_metadata along with the agg_write_block
    bool fsmd_add_evac_fragment(const fs_node_key_t& key,
                                const range& rng,
                                frag_data_t frag,
                                volume_blocks64_t disk_offset,
                                agg_wblock_sync_t& wblock) noexcept final;
    bool fsmd_add_new_fragment(const fs_node_key_t& key,
                               const range& rng,
                               frag_data_t frag,
                               volume_blocks64_t disk_offset,
                               agg_wblock_sync_t& wblock) noexcept final;
    wr_pos fsmd_commit_disk_write(volume_blocks64_t disk_offset,
                                  const std::vector<write_transaction>& wtrans,
                                  agg_wblock_sync_t& wblock) noexcept final;
    void fsmd_fin_flush_commit(volume_blocks64_t disk_offset,
                               const std::vector<write_transaction>& wtrans,
                               agg_wblock_sync_t& wblock) noexcept final;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving the aggregate writer
    bool aggw_try_read_frag(const fs_node_key_t& key,
                            const range_elem& rng,
                            frag_buff_t buff) noexcept final;
    // These two must be called only from the writer thread where
    // the agg_writer lives because these operations are not thread safe.
    bool aggw_write_frag(const frag_write_buff& data,
                         write_transaction& wtrans) noexcept final;
    void aggw_write_final_frag(frag_write_buff&& data,
                               write_transaction&& wtrans) noexcept final;

    ////////////////////////////////////////////////////////////////////////////
    // Temporary, for stats only
    void count_mem_miss() noexcept final;

public:
    bytes64_t data_offs() const noexcept { return data_offset_.to_bytes(); }
    bytes64_t end_data_offs() const noexcept
    {
        return data_offset_.to_bytes() + cnt_data_blocks_.to_bytes();
    }

private:
    expected_t<write_transaction, err_code_t>
    fsmd_begin_write(const object_key& key) noexcept;
    expected_t<write_transaction, err_code_t>
    fsmd_begin_write_truncate(const object_key& key) noexcept;
};

} // namespace detail
} // namespace cache
