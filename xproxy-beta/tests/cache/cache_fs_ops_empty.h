#pragma once

#include "../../cache/cache_fs_ops.h"
#include "../../cache/range_elem.h"
#include "../../cache/read_transaction.h"
#include "../../cache/write_transaction.h"

namespace cache
{
namespace detail
{

class cache_fs_ops_empty : public cache_fs_ops
{
    boost::container::string path_ = "/tmp/cache_fs_ops_empty";

public:
    const boost::container::string& vol_path() const noexcept override
    {
        return path_;
    }

    void report_disk_error() noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }

    ////////////////////////////////////////////////////////////////////////////
    bool vmtx_lock_shared(bytes64_t) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return false;
    }
    void vmtx_unlock_shared() noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
    void vmtx_wait_disk_readers() noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }

    ////////////////////////////////////////////////////////////////////////////
    void aios_push_read_queue(owner_ptr_t<aio_task>) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
    void aios_enqueue_read_queue(owner_ptr_t<aio_task>) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
    bool aios_cancel_task_read_queue(non_owner_ptr_t<aio_task>) noexcept
    {
        X3ME_ASSERT(false, "Must not be called");
        return false;
    }
    void aios_push_write_queue(owner_ptr_t<aio_task>) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
    void aios_enqueue_write_queue(owner_ptr_t<aio_task>) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
    void aios_push_front_write_queue(
        non_owner_ptr_t<agg_writer>) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }

    ////////////////////////////////////////////////////////////////////////////
    read_transaction fsmd_begin_read(const object_key&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return read_transaction{};
    }
    void fsmd_end_read(read_transaction&&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
    write_transaction fsmd_begin_write(const object_key&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return write_transaction{};
    }
    expected_t<range_elem, err_code_t>
    fsmd_find_next_range_elem(const read_transaction&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return boost::make_unexpected(err_code_t{});
    }
    void fsmd_rem_non_evac_frags(std::vector<agg_meta_entry>&,
                                 volume_blocks64_t,
                                 volume_blocks64_t) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }

    ////////////////////////////////////////////////////////////////////////////
    bool fsmd_add_evac_fragment(const fs_node_key_t&, const range&, frag_data_t,
                                volume_blocks64_t,
                                agg_wblock_sync_t&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return false;
    }
    bool fsmd_add_new_fragment(const fs_node_key_t&, const range&, frag_data_t,
                               volume_blocks64_t,
                               agg_wblock_sync_t&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return false;
    }
    wr_pos fsmd_commit_disk_write(volume_blocks64_t,
                                  const std::vector<write_transaction>&,
                                  agg_wblock_sync_t&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return wr_pos{};
    }
    void fsmd_fin_flush_commit(volume_blocks64_t,
                               const std::vector<write_transaction>&,
                               agg_wblock_sync_t&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }

    ////////////////////////////////////////////////////////////////////////////
    bool aggw_try_read_frag(const fs_node_key_t&, const range_elem&,
                            frag_buff_t) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return false;
    }
    bool aggw_write_frag(const frag_write_buff&,
                         write_transaction&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
        return false;
    }
    void aggw_write_final_frag(frag_write_buff&&,
                               write_transaction&&) noexcept override
    {
        X3ME_ASSERT(false, "Must not be called");
    }
};

} // namespace detail
} // namespace cache
