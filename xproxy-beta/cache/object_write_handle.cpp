#include "precompiled.h"
#include "object_write_handle.h"
#include "cache_error.h"
#include "cache_fs_ops.h"
#include "skip_copy.h"

namespace cache
{
namespace detail
{

object_write_handle::object_write_handle(const cache_fs_ops_ptr_t& fso,
                                         const range& actual_rng,
                                         write_transaction&& wtrans) noexcept
    : fs_ops_(fso),
      wtrans_(std::move(wtrans)),
      actual_rng_(actual_rng)
{
    XLOG_DEBUG(disk_tag,
               "Object_write_handle {} created. Obj_key {}. Actual_rng {}",
               log_ptr(this), wtrans_.obj_key(), actual_rng);

    const auto& trng = wtrans_.get_range();
    const auto& arng = actual_rng_;

    X3ME_ENFORCE((arng.beg() <= trng.beg()) && (trng.end() <= arng.end()),
                 "The write transaction range must be equal than or lay inside "
                 "the actual data range");
}

object_write_handle::~object_write_handle() noexcept
{
    XLOG_DEBUG(disk_tag,
               "Object_write_handle {} destroyed. Obj_key {}. Actual_rng {}",
               log_ptr(this), wtrans_.obj_key(), actual_rng_);
    X3ME_ASSERT(
        ((wbuffer_.empty() && !wtrans_.valid()) ||
         (state_.load(std::memory_order_acquire) == state::service_stopped)),
        "The buffer and transaction must have been finalized unless the "
        "service has been stopped");
}

////////////////////////////////////////////////////////////////////////////////

void object_write_handle::async_write(buffers&& bufs,
                                      write_handler_t&& h) noexcept
{
    // The things which are thread safe to be logged in async_write/close
    // methods are the actual range and the write_transaction object_key.
    XLOG_DEBUG(disk_tag, "Object_write_handle {}. Async_write. Obj_key {}",
               log_ptr(this), wtrans_.obj_key());
    // We use with_synchronized here only because we want the assertion
    // to be in the same critical section as the set operation.
    x3me::thread::with_synchronized(
        user_data_,
        [](user_data_t& ud, buffers&& bufs, write_handler_t&& h)
        {
            X3ME_ASSERT(!ud.handler_,
                        "Multiple async operations in flight are not allowed");
            ud.set(std::move(h), std::move(bufs));
        },
        std::move(bufs), std::move(h));
    fs_ops_->aios_push_write_queue(this);
}

void object_write_handle::async_close() noexcept
{
    XLOG_DEBUG(disk_tag, "Object_write_handle {}. Close. Object key {}",
               log_ptr(this), wtrans_.obj_key());

    state curr = state::running;
    if (state_.compare_exchange_strong(curr, state::close))
    {
        // Enqueue the task again for the final write/flush.
        // Using enqueue instead of push because the task could be already
        // in the aio_service queue.
        fs_ops_->aios_enqueue_write_queue(this);
    }

    // If we can get and reset the user data, we may report the operation
    // as aborted. If we can't this means that the operation is executing
    // concurrently or is already executed and done.
    try_fire_error(cache::operation_aborted);
}

////////////////////////////////////////////////////////////////////////////////

void object_write_handle::exec() noexcept
{
    // This call is executed in an AIO thread.
    switch (state_.load(std::memory_order_acquire))
    {
    case state::running:
    {
        if (try_write_all())
        { // Nothing to do here
        }
        else if (state_.load(std::memory_order_acquire) == state::running)
        {
            // We couldn't write all data to the aggregation buffer,
            // we need to schedule write again, if we are still running.
            fs_ops_->aios_push_write_queue(this);
        }
        break;
    }
    case state::close:
        // Try fire operation aborted if we couldn't do it on close.
        // There may not be user handler at all.
        try_fire_error(cache::operation_aborted);
        do_final_write();
        // We need the done state because we can enter here again after the
        // task has done the final write. It's a situation when we detect
        // from the exec function body that we are closed and do the final
        // write, but we still enqueue a new task from the close function.
        state_.store(state::closed, std::memory_order_release);
        break;
    case state::closed:
    case state::service_stopped:
        break;
    default:
        X3ME_ASSERT(false, "Missing switch case");
        break;
    }
}

non_owner_ptr_t<const aio_data> object_write_handle::on_begin_io_op() noexcept
{
    X3ME_ASSERT(false, "This function must not be called. We don't do IO here");
    return nullptr;
}

void object_write_handle::on_end_io_op(const err_code_t&) noexcept
{
    X3ME_ASSERT(false, "This function must not be called. We don't do IO here");
}

void object_write_handle::service_stopped() noexcept
{
    // This function can be executed concurrently from two threads, if the
    // handle write/close is called from one of the network threads,
    // the service_stopped is called from one of the disk threads if
    // the service gets stopped during runtime due to disk errors.
    // Thus we can't do much here, just mark the service as stopped.
    // Note that this call can't be executed concurrently with any of the
    // operations executed in the disk threads - exec, on_begin/end_io_op
    XLOG_DEBUG(disk_tag,
               "Object_write_handle {}. Obj_key {}. Handling service stopped",
               log_ptr(this), wtrans_.obj_key());
    state_.store(state::service_stopped, std::memory_order_release);
    try_fire_error(cache::service_stopped);
}

////////////////////////////////////////////////////////////////////////////////

bool object_write_handle::try_write_all() noexcept
{
    // TODO Possible optimization here is to skip the intermediate copy
    // if we receive all of the expected data at once. We only need
    // a write to the aggregate buffer in this case.

    user_data_t ud;
    user_data_->swap(ud);
    if (ud.empty()) // We've been canceled in the mean time
        return false;

    const auto exp_rng = wtrans_.get_range();
    const auto act_rng = actual_rng_;

    if (wbuffer_.capacity() == 0) // Lazily allocate the write buffer
        wbuffer_ = allocate_wbuff(act_rng.len());

    skip_copy skip_cp{act_rng.len(), processed_bytes_,
                      exp_rng.beg() - act_rng.beg(),
                      act_rng.end() - exp_rng.end()};

    while (!ud.buffers_.all_read() && !skip_cp.done())
    {
        const auto bytes = skip_cp(ud.buffers_, wbuffer_.buff());
        wbuffer_.commit(bytes.copied_);
        XLOG_TRACE(disk_tag, "Object_write_handle {}. Do_write. Consumed "
                             "bytes {}. Skip_copy {}",
                   log_ptr(this), bytes, skip_cp);
        // Write the data to the aggregation buffer if we have a full fragment
        // or we have reached the end of the data.
        // The buffer could be empty if the skip_copy operation skips all.
        if (wbuffer_.full() || (skip_cp.done() && !wbuffer_.empty()))
        {
            if (X3ME_UNLIKELY(wbuffer_.size() > wtrans_.remaining_bytes()))
            {
                std::cerr << "BUG!!! The write buffer doesn't correspond to "
                             "the transaction. WBuffer_size "
                          << wbuffer_.size() << ". WBuffer_cap "
                          << wbuffer_.capacity() << ". Skip_cp_bytes " << bytes
                          << ". Skip_cp_state " << skip_cp
                          << ". Processed_bytes " << processed_bytes_
                          << ". Act_rng " << act_rng << ". Trans " << wtrans_
                          << std::endl;
                ::abort();
            }
            // I don't expect to enter here more than 1 for all cycles.
            if (fs_ops_->aggw_write_frag(wbuffer_, wtrans_))
                wbuffer_.clear();
            else
                break;
        }
    }
    // Note that at this point we may have processed all of the input buffers,
    // but may not have written them to the aggregate buffer because
    // either we don't have enough write buffer to write to or the aggregate
    // writer wasn't able to consume the data from our last write buffer.
    const auto curr_written = skip_cp.curr_offs() - processed_bytes_;
    processed_bytes_        = skip_cp.curr_offs();

    const bool cur_done = ud.buffers_.all_read();
    const bool all_done = skip_cp.done();
    XLOG_DEBUG(disk_tag,
               "Object_write_handle {}. Do_write. Write_trans {}. "
               "Curr_written {}. All_written {}. Curr_done {}. All_done {}",
               log_ptr(this), wtrans_, curr_written, processed_bytes_, cur_done,
               all_done);
    if (cur_done)
    {
        ud.handler_(err_code_t{}, ud.buffers_.bytes_read());
    }
    else if (all_done)
    {
        XLOG_ERROR(disk_tag, "Object_write_handle {}. Do_write. Client has "
                             "provided more data than declared. Write_trans "
                             "{}. Full_range {}",
                   log_ptr(this), wtrans_, act_rng);
        // This is a strange case. We consume the data but return an error
        // which is detected afterwards. However, this shouldn't happen in
        // practice if everything is coded correctly from the above layer.
        const err_code_t err(cache::unexpected_data,
                             get_cache_error_category());
        ud.handler_(err, 0U);
    }
    else
    {
        user_data_->swap(ud); // Restore the user data for the next read
    }
    return cur_done || all_done;
}

void object_write_handle::do_final_write() noexcept
{
    XLOG_DEBUG(disk_tag, "Object_write_handle {}. Write_trans {}. "
                         "Final write. Bytes to write {}",
               log_ptr(this), wtrans_, wbuffer_.size());
    // The aggregate writer will finish write the buffer and finish
    // the transaction. We need to move the buffer to the aggregation writer,
    // because it may need to store it for later,
    // if it has no space to write it currently.
    fs_ops_->aggw_write_final_frag(std::move(wbuffer_), std::move(wtrans_));
}

frag_write_buff
object_write_handle::allocate_wbuff(bytes64_t full_exp_len) const noexcept
{
    // TODO Stats for the allocated bytes in such buffers.
    const bytes32_t buf_cap =
        std::min<bytes64_t>(full_exp_len, object_frag_max_data_size);
    XLOG_DEBUG(disk_tag, "Object_write_handle {}. Obj_key {}. Allocate "
                         "write_buffer {} bytes. All expected bytes {}",
               log_ptr(this), wtrans_.obj_key(), buf_cap, full_exp_len);
    return frag_write_buff{buf_cap};
}

template <typename Err>
void object_write_handle::try_fire_error(Err err) noexcept
{
    user_data_t ud;
    user_data_->swap(ud);
    if (ud.handler_)
        ud.handler_(err_code_t{err, get_cache_error_category()}, 0U);
}

} // namespace detail
} // namespace cache
