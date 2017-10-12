#include "precompiled.h"
#include "cache_fs_operations.h"
#include "agg_writer.h"
#include "agg_write_block.h"
#include "aio_service.h"
#include "cache_error.h"
#include "cache_stats.h"
#include "fs_metadata.h"
#include "object_frag_hdr.h"
#include "object_key.h"
#include "range_vector.h"
#include "read_transaction.h"
#include "volume_fd.h"
#include "write_transaction.h"

namespace cache
{
namespace detail
{

template <typename Stat, typename Val>
static void inc_stat(std::atomic<Stat>& cnt, Val val) noexcept
{
    cnt.fetch_add(val, std::memory_order_release);
}

template <typename Stat>
static Stat read_stat(const std::atomic<Stat>& cnt) noexcept
{
    return cnt.load(std::memory_order_acquire);
}

static cache_fs_ops::wr_pos go_to_next_wr_pos(fs_metadata& md,
                                              bytes64_t data_offs,
                                              bytes64_t end_data_offs) noexcept
{
    if (md.write_pos() + (2 * agg_write_block_size) <= end_data_offs)
        md.inc_write_pos(agg_write_block_size);
    else
        md.wrap_write_pos(data_offs);
    return {md.write_pos(), md.write_lap()};
}

////////////////////////////////////////////////////////////////////////////////

cache_fs_operations::cache_fs_operations(
    non_owner_ptr_t<volume_fd> fd,
    non_owner_ptr_t<fs_metadata_sync_t> md,
    non_owner_ptr_t<aio_service> aios,
    non_owner_ptr_t<const boost::container::string> path,
    store_blocks64_t data_offset,
    store_blocks64_t cnt_data_blocks) noexcept
    : fd_(fd),
      fs_meta_(md),
      aios_(aios),
      path_(path),
      data_offset_(data_offset),
      cnt_data_blocks_(cnt_data_blocks)
{
}

cache_fs_operations::~cache_fs_operations() noexcept
{
}

void cache_fs_operations::set_on_disk_error_cb(
    const on_disk_error_cb_t& cb) noexcept
{
    on_disk_error_cb_ = cb;
}

void cache_fs_operations::set_agg_writer(
    non_owner_ptr_t<agg_writer> agw) noexcept
{
    agg_writer_ = agw;
}

const boost::container::string& cache_fs_operations::vol_path() const noexcept
{
    return *path_;
}

void cache_fs_operations::report_disk_error() noexcept
{
    on_disk_error_cb_();
}

void cache_fs_operations::get_stats(stats_fs_md& smd, stats_fs_ops& sops) const
    noexcept
{
    fs_meta_->as_const()->get_stats(smd, sops);

    sops.data_begin_ = data_offs();
    sops.data_end_   = end_data_offs();
}

void cache_fs_operations::get_internal_stats(stats_internal& sts) const noexcept
{
    auto& is                        = internal_stats_;
    sts.path_                       = *path_;
    sts.cnt_lock_volume_mtx_        = read_stat(is.cnt_lock_volume_mtx_);
    sts.cnt_no_lock_volume_mtx_     = read_stat(is.cnt_no_lock_volume_mtx_);
    sts.cnt_begin_write_ok_         = read_stat(is.cnt_begin_write_ok_);
    sts.cnt_begin_write_fail_       = read_stat(is.cnt_begin_write_fail_);
    sts.cnt_begin_write_trunc_ok_   = read_stat(is.cnt_begin_write_trunc_ok_);
    sts.cnt_begin_write_trunc_fail_ = read_stat(is.cnt_begin_write_trunc_fail_);
    sts.cnt_read_frag_mem_hit_      = read_stat(is.cnt_read_frag_mem_hit_);
    sts.cnt_read_frag_mem_miss_     = read_stat(is.cnt_read_frag_mem_miss_);
    sts.cnt_frag_meta_add_ok_       = read_stat(is.cnt_frag_meta_add_ok_);
    sts.cnt_frag_meta_add_skipped_  = read_stat(is.cnt_frag_meta_add_skipped_);
    sts.cnt_frag_meta_add_limit_    = read_stat(is.cnt_frag_meta_add_limit_);
    sts.cnt_frag_meta_add_overlaps_ = read_stat(is.cnt_frag_meta_add_overlaps_);
    sts.cnt_readers_limit_reached_  = read_stat(is.cnt_readers_limit_reached_);
    sts.cnt_failed_unmark_read_rng_ = read_stat(is.cnt_failed_unmark_read_rng_);
    sts.cnt_invalid_rng_elem_       = read_stat(is.cnt_invalid_rng_elem_);
    sts.cnt_evac_frag_no_mem_entry_ = read_stat(is.cnt_evac_frag_no_mem_entry_);
}

////////////////////////////////////////////////////////////////////////////////
// Operations involving the volume mutex
bool cache_fs_operations::vmtx_lock_shared(bytes64_t disk_offset) noexcept
{
    auto lock_needed = [this](bytes64_t disk_offset)
    {
        constexpr auto agg_write_area_size = 3 * agg_write_block_size;

        const auto doff = data_offs();
        const auto eoff = end_data_offs();
        const auto wpos = fs_meta_->as_const()->write_pos();
        const auto vpos = wpos + agg_write_area_size;

        X3ME_ASSERT(x3me::math::in_range(disk_offset, doff, eoff),
                    "The disk offset must be inside the volume valid area");
        X3ME_ASSERT(x3me::math::in_range(wpos, doff, eoff),
                    "The current write position must be inside the volume "
                    "valid area");

        using x3me::math::in_range;
        if (eoff >= vpos)
        { // Won't go after the end of the disk
            return in_range(disk_offset, wpos, vpos);
        }
        // Translate the end position to a position at the beginning
        const auto end = doff + (vpos % eoff);
        return in_range(disk_offset, doff, end) ||
               in_range(disk_offset, wpos, eoff);
    };
    if (lock_needed(disk_offset))
    {
        inc_stat(internal_stats_.cnt_lock_volume_mtx_, 1);
        vol_mutex_.lock_shared();
        return true;
    }
    inc_stat(internal_stats_.cnt_no_lock_volume_mtx_, 1);
    return false;
}

void cache_fs_operations::vmtx_unlock_shared() noexcept
{
    vol_mutex_.unlock_shared();
}

void cache_fs_operations::vmtx_wait_disk_readers() noexcept
{
    // Used to prevent concurrent disk access when the reads happen in the
    // same region as the write. We are working with O_DIRECT access to
    // a block device and there is nothing to prevent us from concurrent
    // reads and writes, up to my knowledge.
    vol_mutex_.lock();
    vol_mutex_.unlock();
}

////////////////////////////////////////////////////////////////////////////////
// Operations involving the aio_service
void cache_fs_operations::aios_push_read_queue(owner_ptr_t<aio_task> t) noexcept
{
    aios_->push_read_queue(t);
}

void cache_fs_operations::aios_enqueue_read_queue(
    owner_ptr_t<aio_task> t) noexcept
{
    aios_->enqueue_read_queue(t);
}

bool cache_fs_operations::aios_cancel_task_read_queue(
    non_owner_ptr_t<aio_task> t) noexcept
{
    return aios_->cancel_task_read_queue(t);
}

void cache_fs_operations::aios_push_write_queue(
    owner_ptr_t<aio_task> t) noexcept
{
    aios_->push_write_queue(t);
}

void cache_fs_operations::aios_enqueue_write_queue(
    owner_ptr_t<aio_task> t) noexcept
{
    aios_->enqueue_write_queue(t);
}

void cache_fs_operations::aios_push_front_write_queue(
    non_owner_ptr_t<agg_writer> t) noexcept
{
    aios_->push_front_write_queue(t);
}

////////////////////////////////////////////////////////////////////////////////
// Operations involving fs_metadata
read_transaction
cache_fs_operations::fsmd_begin_read(const object_key& key) noexcept
{
    read_transaction ret{key};

    bool limit_reached = false;
    bool set           = false;
    // We lie here a bit. We actually modify the readers count of the found
    // range elements, but the operation is atomic and thus can be run
    // safely with other readers of the same elements.
    fs_meta_->as_const()->read_table_entries(
        key.fs_node_key(), [&key, &set, &limit_reached](const range_vector& rv)
        {
            const auto r = rv.find_full_range(key.get_range());
            if (!r)
                return;
            // Mark read elements
            auto it = r.begin();
            for (; it != r.end(); ++it)
            {
                if (!rv_elem_atomic_inc_readers(it))
                    break; // A limit has been reached.
            }
            if (it != r.end())
            { // Revert the marked once
                for (auto it2 = r.begin(); it2 != it; ++it2)
                    rv_elem_atomic_dec_readers(it2);
                limit_reached = true;
                return;
            }
            set = true;
        });
    // Log outside the synced action above
    if (!set)
    {
        ret.invalidate();
        if (X3ME_UNLIKELY(limit_reached))
        {
            inc_stat(internal_stats_.cnt_readers_limit_reached_, 1);
            XLOG_ERROR(disk_tag,
                       "Unable to begin read for object {}. "
                       "Readers limit reached. This should not happen!!!",
                       key);
        }
    }

    return ret;
}

void cache_fs_operations::fsmd_end_read(read_transaction&& rtrans) noexcept
{
    bool set = false;
    X3ME_ASSERT(rtrans.valid(), "The function accepts only valid transactions");
    // Same small lie here, as in the above function.
    fs_meta_->as_const()->read_table_entries(
        rtrans.fs_node_key(), [&rtrans, &set](const range_vector& rv)
        {
            const auto r = rv.find_full_range(rtrans.get_range());
            set = !r.empty();
            for (auto it = r.begin(); it != r.end(); ++it)
                rv_elem_atomic_dec_readers(it);
        });
    // Log outside the synced action above
    if (X3ME_UNLIKELY(!set))
    {
        inc_stat(internal_stats_.cnt_failed_unmark_read_rng_, 1);
        XLOG_ERROR(disk_tag,
                   "Fs operations. End read. RTrans {}. Unable to "
                   "unmark read ranges. Seems like they've been removed while "
                   "being read. This must not happen. BUG!!!",
                   rtrans);
    }
    rtrans.invalidate();
}

expected_t<write_transaction, err_code_t>
cache_fs_operations::fsmd_begin_write(const object_key& key,
                                      bool truncate_obj) noexcept
{
    return truncate_obj ? fsmd_begin_write_truncate(key)
                        : fsmd_begin_write(key);
}

expected_t<range_elem, err_code_t>
cache_fs_operations::fsmd_find_next_range_elem(
    const read_transaction& rtrans) noexcept
{
    expected_t<range_elem, err_code_t> ret = boost::make_unexpected(
        err_code_t{cache::object_not_present, get_cache_error_category()});

    fs_meta_->as_const()->read_table_entries(
        rtrans.fs_node_key(),
        [ offs = rtrans.curr_offset(), &ret ](const range_vector& rv)
        {
            // We have possible race condition here because the found
            // element can have it's readers concurrently manipulated by the
            // begin/end read functions.
            // However, we are not interested in the exact number of readers
            // of this element and thus the possible race condition here
            // shouldn't be fatal (IMO).
            const range rng{offs, range_elem::min_rng_size(), frag_rng};
            if (const auto found = rv.find_full_range(rng))
                ret = *found.begin();
        });

    if (ret &&
        X3ME_UNLIKELY(!valid_range_elem(ret.value(), data_offset_.to_bytes(),
                                        cnt_data_blocks_.to_bytes())))
    {
        inc_stat(internal_stats_.cnt_invalid_rng_elem_, 1);
        XLOG_ERROR(disk_tag, "Found invalid range_element found upon read. {}",
                   ret.value());
        // TODO We need first a safe mechanic for removing bad entries in
        // a presence of readers. Maybe we can mark them as bad and let the
        // last reader remove them.
        //(*fs_meta_)->rem_table_entry(rtrans.fs_node_key(), ret.value());
        ret = boost::make_unexpected(err_code_t{cache::corrupted_object_meta,
                                                get_cache_error_category()});
    }

    if (ret)
    {
        X3ME_ASSERT(ret->has_readers(),
                    "The found range must have been marked for reading");
    }

    return ret;
}

void cache_fs_operations::fsmd_rem_non_evac_frags(
    std::vector<agg_meta_entry>& entries,
    volume_blocks64_t disk_offs,
    volume_blocks64_t area_size) noexcept
{
    const auto orig_cnt = entries.size();
    // First filter out the entries against the in-memory metadata.
    x3me::thread::with_synchronized(
        *fs_meta_,
        [](fs_metadata& md, std::vector<agg_meta_entry>& entries)
        {
            // Remove fragments without readers from both entries and
            // in-memory metadata.
            for (auto it = entries.begin(); it != entries.end();)
            {
                bool key_found = false;
                md.rem_table_entries(
                    it->key(), [&](range_vector& rv)
                    {
                        bytes64_t rem_size = 0;
                        key_found          = true;
                        const auto key     = it->key();
                        // There could be several successive entries with
                        // the same key.
                        while ((it != entries.end()) && (it->key() == key))
                        {
                            auto rit = rv.find_exact_range(it->rng());
                            // It could happen that an entry read from the
                            // disk is not present in the memory in some rare
                            // cases due to the fact that an entry is first
                            // added to the writer block and later to
                            // the memory and the second operation may fail.
                            if (rit == rv.end())
                                it = entries.erase(it);
                            else if (!rit->has_readers())
                            {
                                rem_size += rit->rng_size();
                                rv.rem_range(rit);
                                it = entries.erase(it);
                            }
                            else
                            {
                                ++it;
                            }
                        }
                        return rem_size;
                    });
                if (!key_found)
                    it = entries.erase(it);
            }
        },
        entries);
    // Second check the remaining entries that they are valid and
    // lay inside the provided disk area.
    const auto doffs = disk_offs.to_bytes();
    const auto dsize = area_size.to_bytes();
    auto new_end = std::remove_if(
        entries.begin(), entries.end(), [doffs, dsize](const auto& e)
        {
            const auto& rng = e.rng();
            const auto offs = rng.disk_offset().to_bytes();
            const auto sz = object_frag_size(rng.rng_size());
            return !valid_range_elem(rng, doffs, dsize) ||
                   !x3me::math::in_range(offs, offs + sz, doffs, doffs + dsize);
        });
    if (X3ME_UNLIKELY(new_end != entries.end()))
    {
        const auto cnt = std::distance(new_end, entries.end());
        inc_stat(internal_stats_.cnt_invalid_rng_elem_, cnt);
        XLOG_ERROR(disk_tag, "Rem_non_evac_frags. Found {} corrupted entries. "
                             "Disk range [{} - {})",
                   cnt, doffs, doffs + dsize);
        entries.erase(new_end, entries.end());
    }
    XLOG_DEBUG(disk_tag, "Rem_non_evac_frags. Removed fragments {}. "
                         "Remaining for evacuation {}",
               orig_cnt - entries.size(), entries.size());
}

////////////////////////////////////////////////////////////////////////////////
// Operations involving fs_metadata along with the agg_write_block
bool cache_fs_operations::fsmd_add_evac_fragment(
    const fs_node_key_t& key,
    const range& rng,
    frag_data_t frag,
    volume_blocks64_t disk_offset,
    agg_wblock_sync_t& wblock) noexcept
{
    // We need to ensure that we use the fs_metadata and the
    // agg_write_block in the same lock order in all places where both
    // of them are used.
    // In addition, we can't use const fs_metadata i.e. read locking because
    // the modifications on the found range element are not atomic/thread
    // safe and they can't run concurrently with other readers.
    bool found_mem = false;
    x3me::thread::with_synchronized(
        *fs_meta_, wblock, [&key, &rng, &frag, &found_mem,
                            disk_offset](fs_metadata& fsm, agg_write_block& awb)
        {
            X3ME_ASSERT(disk_offset.to_bytes() == fsm.write_pos(),
                        "The fragments must be added at the current write "
                        "position");
            const auto ret = awb.add_fragment(key, rng, disk_offset, frag);
            X3ME_ENFORCE(ret, "Adding a fragment evacuated from the disk block "
                              "to the memory block can't fail because of lack "
                              "of space or overlapping");
            // We need to mark the element in memory and change a bit its
            // disk offset to the final place where it's evacuated.
            fsm.modify_table_entries(
                key, [&ret, &found_mem](const range_vector& rv)
                {
                    auto it = rv.find_exact_range(ret.value());
                    if (it != rv.end())
                    {
                        rv_elem_set_in_memory(it, true);
                        rv_elem_set_disk_offset(it, ret->disk_offset());
                        found_mem = true;
                    }
                });
        });
    if (X3ME_UNLIKELY(!found_mem))
    {
        inc_stat(internal_stats_.cnt_evac_frag_no_mem_entry_, 1);
        XLOG_ERROR(
            disk_tag,
            "Evacuated fragment (key: {}, rng: {}) is not found in memory", key,
            rng);
    }
    return found_mem;
}

bool cache_fs_operations::fsmd_add_new_fragment(
    const fs_node_key_t& key,
    const range& rng,
    frag_data_t frag,
    volume_blocks64_t disk_offset,
    agg_wblock_sync_t& wblock) noexcept
{
    optional_t<agg_write_block::fail_res> aggw_add_fail;
    fs_table::add_res fst_add_res = fs_table::add_res::skipped;
    x3me::thread::with_synchronized(
        *fs_meta_, wblock, [&key, &rng, &frag, &aggw_add_fail, &fst_add_res,
                            disk_offset](fs_metadata& fsm, agg_write_block& awb)
        {
            X3ME_ASSERT(disk_offset.to_bytes() == fsm.write_pos(),
                        "The fragments must be added at the current write "
                        "position");
            auto ret = awb.add_fragment(key, rng, disk_offset, frag);
            if (!ret)
            {
                aggw_add_fail = ret.error();
                return;
            }
            // There is a possibility for an entry to be added to the block
            // but failed to be added to the memory metadata.
            // This is bad, but not fatal. Just the entry will remain
            // unknown to the readers
            ret->set_in_memory(true);
            fst_add_res = fsm.add_table_entry(
                key, ret.value(),
                [](range_vector::iter_range overlapped, const range_elem&)
                {
                    // TODO This condition should be improved in the future.
                    // For now overwrite existing ranges if there are no
                    // readers for any of them.
                    for (const auto& o : overlapped)
                        if (o.has_readers())
                            return false;
                    return true;
                });
        });
    // Log outside the critical section.
    // Use switches to see warnings for missing cases if such appear.
    bool res = true;
    if (aggw_add_fail)
    {
        switch (*aggw_add_fail)
        {
        case agg_write_block::fail_res::overlaps:
            inc_stat(internal_stats_.cnt_frag_meta_add_overlaps_, 1);
            XLOG_ERROR(disk_tag,
                       "Skip add new fragment (key: {}, rng: {}). It "
                       "overlaps with existing one(s). This can be fixed",
                       key, rng);
            // Pretend that we have successfully added the fragment, for
            // now. Let's first see how often will happen to write overlapping
            // fragments at the same time.
            break;
        case agg_write_block::fail_res::no_space_meta:
        case agg_write_block::fail_res::no_space_data:
            XLOG_DEBUG(disk_tag, "Skip add new fragment (key: {}, rng: {}). {}",
                       key, rng, *aggw_add_fail);
            res = false;
            break;
        }
        return res;
    }
    switch (fst_add_res)
    {
    case fs_table::add_res::added:
    case fs_table::add_res::overwrote:
        inc_stat(internal_stats_.cnt_frag_meta_add_ok_, 1);
        XLOG_DEBUG(disk_tag, "Added new fragment (key: {}, rng: {}). Res {}",
                   key, rng, fst_add_res);
        break;
    case fs_table::add_res::limit_reached:
        // Once limit is reached we'll start writing fewer entries to the
        // metadata then move to the next aggregate_block where we hope
        // evacuate as fewer as possible fragments and thus free space in
        // the metadata for new fragments.
        inc_stat(internal_stats_.cnt_frag_meta_add_limit_, 1);
        XLOG_ERROR(disk_tag, "Skip add new fragment (key: {}, rng: {}). "
                             "Metadata limit reached",
                   key, rng);
        res = false;
        break;
    case fs_table::add_res::skipped:
        inc_stat(internal_stats_.cnt_frag_meta_add_skipped_, 1);
        XLOG_ERROR(disk_tag, "Skip add metadata for new fragment (key: {}, "
                             "rng: {}). Won't overwrite a read fragment. Res "
                             "{}",
                   key, rng, fst_add_res);
        break; // We want to return true as if the fragment has been added
    }
    return res;
}

// Used in the logging in the commit_writes
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& rhs) noexcept
{
    os << rhs.size() << ":[";
    for (const auto& e : rhs)
        os << e << ';';
    os << ']';
    return os;
}

cache_fs_ops::wr_pos cache_fs_operations::fsmd_commit_disk_write(
    volume_blocks64_t disk_offset,
    const std::vector<write_transaction>& wtrans,
    agg_wblock_sync_t& wblock) noexcept
{
    uint32_t not_found = 0;
    std::vector<agg_meta_entry> writes;
    const auto wpos = x3me::thread::with_synchronized(
        *fs_meta_, wblock,
        [disk_offset, &writes,
         &not_found](fs_metadata& md, agg_write_block& awb, bytes64_t data_offs,
                     bytes64_t end_data_offs)
        {
            // Commit the current write to the write block
            writes = awb.end_disk_write();
            /* Skip write transactions functionality for now
            // Process the finished transactions
            for (const auto& wt : wtrans)
                wtranss.rem_entry(wt);
            */
            // Mark the written entries that they are on the disk
            for (auto it = writes.begin(); it != writes.end();)
            {
                const bool key_found = md.modify_table_entries(
                    it->key(), [&](const range_vector& rv)
                    {
                        const auto key = it->key();
                        // There could be several successive entries with
                        // the same key.
                        for (; (it != writes.end()) && (it->key() == key); ++it)
                        {
                            auto rit = rv.find_exact_range(it->rng());
                            // It could happen that an entry read from the
                            // disk is not present in the memory in some rare
                            // cases due to the fact that an entry is first
                            // added to the writer block and later to
                            // the memory and the second operation may fail.
                            if (rit != rv.end())
                                rv_elem_set_in_memory(rit, false);
                            else
                                ++not_found;
                        }
                    });
                if (!key_found)
                    ++it;
            }
            // Finally move the write position forward
            X3ME_ASSERT(disk_offset.to_bytes() == md.write_pos(),
                        "The fragments must be added at the current write "
                        "position");
            return go_to_next_wr_pos(md, data_offs, end_data_offs);
        },
        data_offs(), end_data_offs());
    // Log the writes and destroy them outside of the critical section
    XLOG_DEBUG(disk_tag,
               "Commit_writes. Write_transs: {}. Not_found: {}. Written: {}",
               wtrans, not_found, writes);
    return wpos;
}

void cache_fs_operations::fsmd_fin_flush_commit(
    volume_blocks64_t disk_offset,
    const std::vector<write_transaction>& wtrans,
    agg_wblock_sync_t& wblock) noexcept
{
    XLOG_DEBUG(disk_tag, "Fin_commit_writes begin. Write_transs: {}", wtrans);

    const bool commit_write = x3me::thread::with_synchronized(
        *fs_meta_, wblock,
        [](fs_metadata& fsm, agg_write_block& awb, volume_fd& fd,
           bytes64_t end_data_offs)
        {
            bool ret = false;
            if (awb.bytes_avail() > 0)
            {
                stats_fs_wr unused;
                const auto buff = awb.begin_disk_write(unused);

                // We don't need to worry about writing past the end of the
                // volume here. We should have checked this when start
                // working with the current write block.
                const auto wpos     = fsm.write_pos();
                const auto end_wpos = wpos + agg_write_block_size;
                X3ME_ASSERT((end_wpos <= end_data_offs) &&
                            "Incorrect write_pos");
                X3ME_ASSERT((buff.size() <= agg_write_block_size) &&
                            "Invalid buffer size");
                err_code_t err;
                if (fd.write(buff.data(), buff.size(), wpos, err))
                {
                    ret = true;
                }
                else
                {
                    XLOG_ERROR(disk_tag, "Fin_commit_writes. Failed to write "
                                         "the final aggregate block data to "
                                         "the disk. Offs: {} bytes. Size: {} "
                                         "bytes. Error: {}",
                               wpos, buff.size(), err.message());
                }
            }
            return ret;
        },
        *fd_, end_data_offs());

    if (commit_write)
        fsmd_commit_disk_write(disk_offset, wtrans, wblock);
    // TODO We need a safe mechanic for rollback/remove unsaved entries,
    // not only on final write (which is easier) but also on runtime write
    // failure.
}

////////////////////////////////////////////////////////////////////////////////
// Operations involving the aggregate writer
bool cache_fs_operations::aggw_try_read_frag(const fs_node_key_t& key,
                                             const range_elem& rng,
                                             frag_buff_t buff) noexcept
{
    // We intentionally use the metadata inside the critical section because
    // we don't want racy situations where the current write position is
    // changed after we've asked the fs_metadata.
    // The aggregate writer has the current write position but it's not
    // currently thread safe to use it.
    // In addition, this method should not be heavily hit because it should
    // be used only when a reader is interested in a fragment which currently
    // lays inside the agg_writer buffer, which should be pretty rare (IMO).
    const bool res = x3me::thread::with_synchronized(
        fs_meta_->as_const(), agg_writer_->write_block().as_const(),
        [&](const fs_metadata& fsm, const agg_write_block& awb)
        {
            const auto wpos =
                volume_blocks64_t::create_from_bytes(fsm.write_pos());
            return awb.try_read_fragment(key, rng, wpos, buff);
        });
    if (res)
        inc_stat(internal_stats_.cnt_read_frag_mem_hit_, 1);
    else
        inc_stat(internal_stats_.cnt_read_frag_mem_miss_, 1);
    return res;
}

bool cache_fs_operations::aggw_write_frag(const frag_write_buff& data,
                                          write_transaction& wtrans) noexcept
{
    return agg_writer_->write(data, wtrans);
}

void cache_fs_operations::aggw_write_final_frag(
    frag_write_buff&& data, write_transaction&& wtrans) noexcept
{
    agg_writer_->final_write(std::move(data), std::move(wtrans));
}

////////////////////////////////////////////////////////////////////////////////
// Temporary, for stats only
void cache_fs_operations::count_mem_miss() noexcept
{
    inc_stat(internal_stats_.cnt_read_frag_mem_miss_, 1);
}

////////////////////////////////////////////////////////////////////////////////

expected_t<write_transaction, err_code_t>
cache_fs_operations::fsmd_begin_write(const object_key& key) noexcept
{
    const auto new_rng = x3me::thread::with_synchronized(
        fs_meta_->as_const(),
        [](const fs_metadata& md, const object_key& key)
        {
            range new_rng = key.get_range();
            md.read_table_entries(key.fs_node_key(), [&](const range_vector& rv)
                                  {
                                      new_rng = rv.trim_overlaps(new_rng);
                                  });
            // TODO Don't use as_const() above if you start using
            // write_transactions functionality because as_const() would
            // imply shared_lock to be used, but we modify the
            // write_transactions adding new entry.
            // if (!new_rng.empty())
            //    return wtranss.add_entry(key.fs_node_key(), new_rng);
            return new_rng;
        },
        key);
    XLOG_DEBUG(disk_tag, "Begin_write. Object_key: {}. Trimmed range {}", key,
               new_rng);
    if (new_rng.len() == 0)
    {
        inc_stat(internal_stats_.cnt_begin_write_fail_, 1);
        return boost::make_unexpected(
            cache::make_error_code(cache::object_present));
    }
    else if (new_rng.len() < cache::detail::min_obj_size)
    {
        inc_stat(internal_stats_.cnt_begin_write_fail_, 1);
        return boost::make_unexpected(
            cache::make_error_code(cache::new_object_too_small));
    }
    inc_stat(internal_stats_.cnt_begin_write_ok_, 1);
    return write_transaction{key.fs_node_key(), new_rng};
}

expected_t<write_transaction, err_code_t>
cache_fs_operations::fsmd_begin_write_truncate(const object_key& key) noexcept
{
    const auto found_removed = x3me::thread::with_synchronized(
        *fs_meta_,
        [](fs_metadata& md, const object_key& key)
        {
            return md.rem_table_entries(
                key.fs_node_key(), [](range_vector& rv)
                {
                    bytes64_t rem_size = 0;

                    for (const auto& r : rv)
                    {
                        if (r.has_readers())
                            return rem_size;
                    }

                    auto beg = rv.begin();
                    auto end = rv.end();

                    rem_size =
                        std::accumulate(beg, end, rem_size,
                                        [](bytes64_t sum, const range_elem& rng)
                                        {
                                            return sum + rng.rng_size();
                                        });

                    rv.rem_range(range_vector::iter_range{beg, end});

                    return rem_size;
                });
            // if (!new_rng.empty())
            //    return wtranss.add_entry(key.fs_node_key(), new_rng);
        },
        key);
    const bool truncated = !found_removed || (found_removed.value() > 0);
    XLOG_DEBUG(disk_tag,
               "Begin_write_truncate. Object_key: {}. Found {}. Truncated {}",
               key, !!found_removed, truncated);
    if (!truncated)
    {
        inc_stat(internal_stats_.cnt_begin_write_trunc_fail_, 1);
        return boost::make_unexpected(
            cache::make_error_code(cache::object_in_use));
    }
    inc_stat(internal_stats_.cnt_begin_write_trunc_ok_, 1);
    return write_transaction{key.fs_node_key(), key.get_range()};
}

} // namespace detail
} // namespace cache
