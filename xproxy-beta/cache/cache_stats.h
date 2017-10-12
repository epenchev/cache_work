#pragma once

namespace cache
{

struct stats_fs_md
{
    uint64_t cnt_entries_            = 0;
    uint32_t cnt_nodes_              = 0;
    uint32_t cnt_ranges_             = 0;
    bytes32_t curr_data_size_        = 0;
    bytes32_t max_allowed_data_size_ = 0;
    bytes64_t entries_data_size_     = 0;
};

struct stats_fs_wr
{
    bytes64_t written_meta_size_ = 0;
    bytes64_t wasted_meta_size_  = 0;
    bytes64_t written_data_size_ = 0;
    bytes64_t wasted_data_size_  = 0;

    uint64_t cnt_block_meta_read_ok_   = 0;
    uint64_t cnt_block_meta_read_err_  = 0;
    uint64_t cnt_evac_entries_checked_ = 0;
    uint64_t cnt_evac_entries_todo_    = 0;
    uint64_t cnt_evac_entries_ok_      = 0;
    uint64_t cnt_evac_entries_err_     = 0;
};

struct stats_fs_ops
{
    bytes64_t data_begin_ = 0;
    bytes64_t data_end_   = 0;
    bytes64_t write_pos_  = 0;
    uint64_t write_lap_   = 0;
};

struct stats_fs : stats_fs_ops, stats_fs_md, stats_fs_wr
{
    boost::container::string path_;

    uint32_t cnt_pending_reads_  = 0;
    uint32_t cnt_pending_writes_ = 0;

    uint16_t cnt_errors_ = 0;
};

struct stats_internal
{
    boost::container::string path_;

    uint64_t cnt_lock_volume_mtx_    = 0;
    uint64_t cnt_no_lock_volume_mtx_ = 0;

    uint64_t cnt_begin_write_ok_         = 0;
    uint64_t cnt_begin_write_fail_       = 0;
    uint64_t cnt_begin_write_trunc_ok_   = 0;
    uint64_t cnt_begin_write_trunc_fail_ = 0;

    uint64_t cnt_read_frag_mem_hit_  = 0;
    uint64_t cnt_read_frag_mem_miss_ = 0;

    uint64_t cnt_frag_meta_add_ok_      = 0;
    uint64_t cnt_frag_meta_add_skipped_ = 0;
    // These two errors should happen often, and hopefully won't happen at all
    uint32_t cnt_frag_meta_add_limit_    = 0;
    uint32_t cnt_frag_meta_add_overlaps_ = 0;

    // These error counters should remain 0 or really close to 0
    uint32_t cnt_readers_limit_reached_  = 0;
    uint32_t cnt_failed_unmark_read_rng_ = 0;
    uint32_t cnt_invalid_rng_elem_       = 0;
    uint32_t cnt_evac_frag_no_mem_entry_ = 0;
};

} // namespace cache
