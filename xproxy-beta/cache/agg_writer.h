#pragma once

#include "aio_data.h"
#include "aio_task.h"
#include "agg_write_block.h"
#include "cache_fs_ops_fwds.h"

namespace cache
{
struct stats_fs_wr;
namespace detail
{
class frag_write_buff;
class write_transaction;
namespace awsm
{
struct ev_io_done;
struct ev_do_write;
struct ev_do_fin_write;
class sm;
struct sm_impl;
struct state_data;
} // namespace awsm
////////////////////////////////////////////////////////////////////////////////
class agg_writer final : public aio_task
{
    friend struct awsm::sm_impl;

    struct stats
    {
        std::atomic<bytes64_t> written_meta_size_{0};
        std::atomic<bytes64_t> wasted_meta_size_{0};
        std::atomic<bytes64_t> written_data_size_{0};
        std::atomic<bytes64_t> wasted_data_size_{0};
        std::atomic<uint64_t> cnt_block_meta_read_ok_{0};
        std::atomic<uint64_t> cnt_block_meta_read_err_{0};
        std::atomic<uint64_t> cnt_evac_entries_checked_{0};
        std::atomic<uint64_t> cnt_evac_entries_todo_{0};
        std::atomic<uint64_t> cnt_evac_entries_ok_{0};
        std::atomic<uint64_t> cnt_evac_entries_err_{0};
    };

    non_owner_ptr_t<cache_fs_ops> fs_ops_;

    // The only part of the agg_writer which is accessed by multiple threads
    // is the agg_write_block. Some readers may need content which is currently
    // written to the agg_write_block buffer, but is still not flushed to the
    // cache.
    agg_wblock_sync_t write_block_;

    x3me::utils::pimpl<awsm::sm, 32, 8> sm_;
    x3me::utils::pimpl<awsm::state_data, 112, 8> sdata_;

    // Transactions finished in the current aggregate write pass.
    std::vector<write_transaction> finished_trans_;

    stats stats_;

    aio_data aio_data_;
    aio_op aio_op_ = aio_op::exec;

public:
    agg_writer(volume_blocks64_t write_pos, uint64_t write_lap) noexcept;
    ~agg_writer() noexcept final;

    agg_writer(const agg_writer&) = delete;
    agg_writer& operator=(const agg_writer&) = delete;
    agg_writer(agg_writer&&) = delete;
    agg_writer& operator=(agg_writer&&) = delete;

    void start(non_owner_ptr_t<cache_fs_ops> fso) noexcept;
    // Stops the writer flushing its memory buffer to the disk if needed.
    void stop_flush() noexcept;

    // Returns true if the passed write_buffer is consumed and false otherwise.
    // The write_buffer is either fully consumed or not.
    // There is no case with partial consumption.
    bool write(const frag_write_buff& wbuf, write_transaction& wtrans) noexcept;
    void final_write(frag_write_buff&& wbuf,
                     write_transaction&& wtrans) noexcept;

    // The method can be safely called from multiple threads
    void get_stats(stats_fs_wr& swr) noexcept;

    const agg_wblock_sync_t& write_block() const noexcept
    {
        return write_block_;
    }
#ifdef X3ME_TEST
    agg_wblock_sync_t& write_block() noexcept { return write_block_; }
#endif

private:
    aio_op operation() const noexcept final { return aio_op_; }

    void exec() noexcept final;

    non_owner_ptr_t<const aio_data> on_begin_io_op() noexcept final;
    void on_end_io_op(const err_code_t& err) noexcept final;

    // We don't need to do anything here
    void service_stopped() noexcept final {}

private: // Action handlers
    void enqueue_read_aio_op() noexcept;
    void enqueue_write_aio_op() noexcept;
    void begin_md_read() noexcept;
    void on_md_read(const awsm::ev_io_done& ev) noexcept;
    void begin_evac() noexcept;
    void on_evac_done(const awsm::ev_io_done& ev) noexcept;
    void write_pend_data() noexcept;
    void do_write(const awsm::ev_do_write& ev) noexcept;
    void do_fin_write(awsm::ev_do_fin_write& ev) noexcept;
    bool do_write_impl(write_transaction& wtrans,
                       const frag_write_buff& wbuf,
                       bool fin_write) noexcept;
    void begin_flush() noexcept;
    void on_flush_done(const awsm::ev_io_done& ev) noexcept;
    void do_last_flush() noexcept;
};

using agg_writer_ptr_t = aio_task_ptr_t<agg_writer>;

} // namespace detail
} // namespace cache
