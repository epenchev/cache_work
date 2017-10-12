#pragma once

// TODO These includes could be removed if the corresponding typedefs
// become classes.
#include "cache_fs_ops_fwds.h"
#include "fs_node_key.h"
#include "unit_blocks.h"

namespace cache
{
namespace detail
{
class aio_task;
class agg_meta_entry;
class agg_writer;
class frag_write_buff;
class object_key;
class range;
class range_elem;
class read_transaction;
class write_transaction;

// This interface facilitates decoupling of various cache components from
// the knowledge for cache_fs and the needed stuff which lives there.
// It also facilitates the unit testing of the cache components.
struct cache_fs_ops
{
    virtual ~cache_fs_ops() noexcept {}

    virtual const boost::container::string& vol_path() const noexcept = 0;

    virtual void report_disk_error() noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////
    // These three functions are my pain, really.
    // Couldn't figure out a better way to ensure that the currently written
    // region of the volume is not accessed by readers at the same time.
    // The first function locks the volume mutex depending of the disk_offset
    // value. Returns true if the mutex is locked and false otherwise.
    // If the mutex is locked a call to vmtx_unlock_shared must be issued later.
    virtual bool vmtx_lock_shared(bytes64_t disk_offset) noexcept = 0;
    virtual void vmtx_unlock_shared() noexcept = 0;
    virtual void vmtx_wait_disk_readers() noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving the aio_service
    virtual void aios_push_read_queue(owner_ptr_t<aio_task>) noexcept = 0;
    virtual void aios_enqueue_read_queue(owner_ptr_t<aio_task>) noexcept = 0;
    virtual bool
        aios_cancel_task_read_queue(non_owner_ptr_t<aio_task>) noexcept = 0;
    virtual void aios_push_write_queue(owner_ptr_t<aio_task>) noexcept = 0;
    virtual void aios_enqueue_write_queue(owner_ptr_t<aio_task>) noexcept = 0;
    // It's important that only the aggregate_writer can push itself at the
    // front of the queue, because this way it ensures that the other write
    // tasks wait until it finishes a disk write operation.
    virtual void
        aios_push_front_write_queue(non_owner_ptr_t<agg_writer>) noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving fs_metadata
    virtual read_transaction fsmd_begin_read(const object_key&) noexcept = 0;
    virtual void fsmd_end_read(read_transaction&& rtrans) noexcept = 0;
    virtual expected_t<write_transaction, err_code_t>
    fsmd_begin_write(const object_key&, bool truncate_obj) noexcept = 0;
    virtual expected_t<range_elem, err_code_t>
    fsmd_find_next_range_elem(const read_transaction&) noexcept = 0;
    virtual void fsmd_rem_non_evac_frags(std::vector<agg_meta_entry>&,
                                         volume_blocks64_t,
                                         volume_blocks64_t) noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving fs_metadata along with the agg_write_block
    using frag_data_t = x3me::mem_utils::array_view<const uint8_t>;
    virtual bool fsmd_add_evac_fragment(const fs_node_key_t&,
                                        const range&,
                                        frag_data_t,
                                        volume_blocks64_t,
                                        agg_wblock_sync_t&) noexcept = 0;
    virtual bool fsmd_add_new_fragment(const fs_node_key_t&,
                                       const range&,
                                       frag_data_t,
                                       volume_blocks64_t,
                                       agg_wblock_sync_t&) noexcept = 0;
    struct wr_pos
    {
        bytes64_t write_pos_;
        uint64_t write_lap_;
    };
    virtual wr_pos fsmd_commit_disk_write(volume_blocks64_t,
                                          const std::vector<write_transaction>&,
                                          agg_wblock_sync_t&) noexcept = 0;
    virtual void fsmd_fin_flush_commit(volume_blocks64_t,
                                       const std::vector<write_transaction>&,
                                       agg_wblock_sync_t&) noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Operations involving the aggregate writer
    using frag_buff_t = x3me::mem_utils::array_view<uint8_t>;
    virtual bool aggw_try_read_frag(const fs_node_key_t&,
                                    const range_elem&,
                                    frag_buff_t) noexcept = 0;
    virtual bool aggw_write_frag(const frag_write_buff&,
                                 write_transaction&) noexcept = 0;
    virtual void aggw_write_final_frag(frag_write_buff&&,
                                       write_transaction&&) noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Temporary, for stats only
    virtual void count_mem_miss() noexcept = 0;
};

} // namespace detail
} // namespace cache
