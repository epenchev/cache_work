#include "precompiled.h"
#include "object_read_handle.h"
#include "cache_error.h"
#include "cache_fs_ops.h"
#include "object_frag_hdr.h"

namespace cache
{
namespace detail
{

static object_frag_hdr frag_hdr(const aligned_data_ptr_t& buf) noexcept
{
    static_assert(std::is_trivial<object_frag_hdr>::value,
                  "Needs to be trivial for the memcpy");
    object_frag_hdr hdr;
    ::memcpy(&hdr, buf.get(), sizeof(hdr));
    return hdr;
}

static const uint8_t* frag_data(const aligned_data_ptr_t& buf) noexcept
{
    return buf.get() + sizeof(object_frag_hdr);
}

static void expand_frag_buff_if_needed(aligned_data_ptr_t& buff,
                                       bytes32_t& cur_size,
                                       bytes32_t new_size) noexcept
{
    // Once allocated the buffer for the biggest object fragment it won't be
    // freed if only smaller fragments follow after it.
    if (new_size > cur_size)
    {
        buff     = alloc_page_aligned(new_size);
        cur_size = new_size;
    }
}

////////////////////////////////////////////////////////////////////////////////

void object_read_handle::user_data::set_uh_buffers(read_handler_t&& h,
                                                   buffers&& wb) noexcept
{
    uh_buffers_.set(std::move(h), std::move(wb));
}

void object_read_handle::user_data::swap_uh_buffers(uh_buffers_t& rhs) noexcept
{
    uh_buffers_.swap(rhs);
}

void object_read_handle::user_data::set_close_handler(
    close_handler_ptr_t&& h) noexcept
{
    close_handler_ = std::move(h);
}

object_read_handle::user_data::close_handler_ptr_t
object_read_handle::user_data::release_close_handler() noexcept
{
    return std::move(close_handler_);
}

////////////////////////////////////////////////////////////////////////////////

object_read_handle::object_read_handle(const cache_fs_ops_ptr_t& fso,
                                       read_transaction&& rtrans) noexcept
    : fs_ops_(fso),
      rtrans_(std::move(rtrans))
{
    XLOG_DEBUG(disk_tag, "Object_read_handle {} created. RTrans {}",
               log_ptr(this), rtrans_);
    X3ME_ASSERT(rtrans_.valid(), "The read transaction must be valid");
}

object_read_handle::~object_read_handle()
{
    XLOG_DEBUG(disk_tag, "Object_read_handle {} destroyed. RTrans {}",
               log_ptr(this), rtrans_);
    X3ME_ASSERT(!rtrans_.valid() || (state_.load(std::memory_order_acquire) ==
                                     state::service_stopped),
                "The read transaction must have been finalized unless the "
                "service has been stopped");
}

void object_read_handle::async_read(buffers&& bufs, read_handler_t&& h) noexcept
{
    XLOG_DEBUG(disk_tag, "Object_read_handle {}. Async_read. Obj_key {}",
               log_ptr(this), rtrans_.obj_key());
    user_data_->set_uh_buffers(std::move(h), std::move(bufs));
    fs_ops_->aios_push_read_queue(this);
}

void object_read_handle::async_close(close_handler_t&& h) noexcept
{
    // Note we can't log other data except the object key, because it'd be
    // thread unsafe.
    XLOG_DEBUG(disk_tag,
               "Object_read_handle {}. Close with handler. Object key {}",
               log_ptr(this), rtrans_.obj_key());

    state curr = state::running;
    if (state_.compare_exchange_strong(curr, state::close))
    {
        user_data_->set_close_handler(
            std::make_unique<close_handler_t>(std::move(h)));

        // Enqueue the task again so that we can end the read transaction
        // in the read thread(s). Using enqueue instead of push because the task
        // could be already in the aio_service queue.
        fs_ops_->aios_enqueue_read_queue(this);
        // If we can get and reset the user data, we may report the operation
        // as aborted. If we can't this means that the operation is executing
        // concurrently or is already executed and done.
        try_fire_error(cache::operation_aborted);
    }
    else // The handle has already been closed or it's in process of closing
    {
        try_fire_error(cache::operation_aborted);
        h(cache::success);
    }
}

void object_read_handle::async_close() noexcept
{
    // Note we can't log other data except the object key, because it'd be
    // thread unsafe.
    XLOG_DEBUG(disk_tag, "Object_read_handle {}. Close. Object key {}",
               log_ptr(this), rtrans_.obj_key());

    state curr = state::running;
    if (state_.compare_exchange_strong(curr, state::close))
    {
        // Enqueue the task again so that we can end the read transaction
        // in the read thread(s). Using enqueue instead of push because the task
        // could be already in the aio_service queue.
        fs_ops_->aios_enqueue_read_queue(this);
    }

    // If we can get and reset the user data, we may report the operation
    // as aborted. If we can't this means that the operation is executing
    // concurrently or is already executed and done.
    try_fire_error(cache::operation_aborted);
}

////////////////////////////////////////////////////////////////////////////////

void object_read_handle::exec() noexcept
{
    X3ME_ASSERT(false, "Exec must not be called. We do only IO operations");
}

non_owner_ptr_t<const aio_data> object_read_handle::on_begin_io_op() noexcept
{
    // This call is executed in an AIO thread. Returning a nullptr tells
    // the aio_service that we want to skip the IO operation.
    non_owner_ptr_t<const aio_data> ret = nullptr;

    if (!serializator_.try_lock())
    {
        // The on_begin/end_io_op section is currently executing. We need to
        // retry later. This can happen durring the close operations.
        // We may enter here also if the on_end_io_op function enqueues the
        // read task again and it's picked from another read thread before
        // the on_end_io_op is actually done and unlocked the serializator.
        // We need to use enqueue instead of push because a close operation
        // could be already in the queue in the same time.
        fs_ops_->aios_enqueue_read_queue(this);
        return ret;
    }
    X3ME_SCOPE_EXIT
    {
        // If we skip the IO operation, the on_end_io_op won't be called.
        // Thus we need to unlock the serializator here.
        if (!ret)
            serializator_.unlock();
    };

    switch (state_.load(std::memory_order_acquire))
    {
    case state::running:
    {
        while (try_read_all_from_mem_buff() == read_res::end_of_buf)
        {
            const auto r = begin_io_op();
            if ((ret = r.aio_data_))
                break; // We need to start IO operation
            else if (!r.try_read_mem_)
                break; // There's been an error we just need to exit.
            // Otherwise try again to read data which just has been loaded
            // to the memory buffer.
        }
        break;
    }
    case state::close:
        // Try fire operation aborted if we couldn't do it on close.
        // There may not be user handler at all.
        read_handle_done();
        try_fire_error(cache::operation_aborted);
        try_fire_closed(cache::success);
        break;
    case state::closed:
        try_fire_error(cache::invalid_handle);
        try_fire_closed(cache::success);
        break;
    case state::service_stopped:
        break;
    default:
        X3ME_ASSERT(false, "Missing switch case");
        break;
    }
    return ret;
}

object_read_handle::begin_io_op_res object_read_handle::begin_io_op() noexcept
{
    begin_io_op_res ret{nullptr /*skip IO*/, false /*Not in memory*/};

    // This call returns a valid range or no range at all.
    const auto new_rng = fs_ops_->fsmd_find_next_range_elem(rtrans_);
    if (!new_rng)
    {
        XLOG_ERROR(disk_tag, "Unable to get next valid range_elem for "
                             "read_transaction. {}. Object_read_handle {}. FS "
                             "'{}'. RTrans {}",
                   new_rng.error().message(), log_ptr(this),
                   fs_ops_->vol_path(), rtrans_);
        read_handle_done();
        try_fire_error(new_rng.error().value());
        try_fire_closed(cache::success);
        return ret;
    }

    const auto aligned_size = object_frag_size(new_rng->rng_size());

    expand_frag_buff_if_needed(frag_read_buff_, read_buff_size_, aligned_size);
    curr_rng_ = new_rng.value();

    // Even if the entry says it's in_memory the situation is racy and we may
    // not be able to find it in the aggregate writer memory block.
    // It could be committed to the disk between the first check here and the
    // second call.
    const bool in_mem = new_rng->in_memory();
    if (in_mem &&
        fs_ops_->aggw_try_read_frag(
            rtrans_.fs_node_key(), new_rng.value(),
            cache_fs_ops::frag_buff_t{frag_read_buff_.get(), aligned_size}))
    {
        if (!check_read_data())
        {
            XLOG_ERROR(disk_tag, "Wrong fragment data after memory read. "
                                 "Object_read_handle {}. FS '{}'. RTrans {}",
                       log_ptr(this), fs_ops_->vol_path(), rtrans_);
            read_handle_done();
            try_fire_error(cache::corrupted_object_data);
            try_fire_closed(cache::success);
            return ret;
        }
        // Set this to 0 to catch potential mistakes easily.
        aio_data_.buf_    = nullptr;
        aio_data_.offs_   = 0;
        aio_data_.size_   = 0;
        ret.try_read_mem_ = true;
        return ret;
    }
    else if (!in_mem)
    {
        fs_ops_->count_mem_miss(); // TODO Temporary for stats only
    }

    // We may not need all the data of the last object fragment,
    // but the logic becomes too complicated when we take into account that
    // a whole transaction may lay inside a given range_element and we may
    // need to skip bytes from both ends of the range data.
    aio_data_.size_ = aligned_size;
    aio_data_.buf_  = frag_read_buff_.get();
    aio_data_.offs_ = new_rng->disk_offset().to_bytes();

    X3ME_ASSERT(
        !vol_mutex_locked_,
        "Wrong logic around locking/unlocking of the volume access mutex");
    vol_mutex_locked_ = fs_ops_->vmtx_lock_shared(aio_data_.offs_);

    XLOG_DEBUG(disk_tag, "Begin disk read. Object_read_handle {}. Disk_offs "
                         "{}. Size {}. Rng {}. VolMutex locked {}",
               log_ptr(this), aio_data_.offs_, aio_data_.size_, *new_rng,
               vol_mutex_locked_);

    ret.aio_data_ = &aio_data_;
    return ret;
}

void object_read_handle::on_end_io_op(const err_code_t& err) noexcept
{
    // Unlock the serializator no matter what happens here
    X3ME_SCOPE_EXIT { serializator_.unlock(); };

    XLOG_TRACE(disk_tag, "End disk read. Object_read_handle {}. Disk_offs "
                         "{}. VolMutex locked {}",
               log_ptr(this), aio_data_.offs_, vol_mutex_locked_);
    if (vol_mutex_locked_)
    {
        fs_ops_->vmtx_unlock_shared();
        vol_mutex_locked_ = false;
    }

    // TODO We can decrease the readers here for the currently read fragment.
    // This can prevent unneeded evacuations.

    if (err)
    {
        read_handle_done();
        try_fire_error(cache::disk_error);
        try_fire_closed(cache::success);
        report_disk_error(err, aio_data_.offs_, aio_data_.size_);
        return;
    }

    if (!check_read_data())
    {
        XLOG_ERROR(disk_tag, "Wrong fragment data after disk read. "
                             "Object_read_handle {}. FS '{}'. RTrans {}",
                   log_ptr(this), fs_ops_->vol_path(), rtrans_);
        read_handle_done();
        try_fire_error(cache::corrupted_object_data);
        try_fire_closed(cache::success);
        return;
    }

    if (try_read_all_from_mem_buff() != read_res::end_of_buf)
    { // All read or aborted
    }
    else if (state_.load(std::memory_order_acquire) == state::running)
    {
        // We couldn't read all of the user wanted data. Usually the user
        // provided buffers are much smaller than the fragment buffer and
        // we'll enter here very rarely if at all. We need to read more data
        // again in order to fill the whole user buffer, or reach EOF.
        // We need to use enqueue instead of push because a close operation
        // could be already in the queue in the same time.
        fs_ops_->aios_enqueue_read_queue(this);
    }
}

void object_read_handle::service_stopped() noexcept
{
    // This function can be executed concurrently from two threads, if the
    // handle write/close is called from one of the network threads,
    // the service_stopped is called from one of the disk threads if
    // the service gets stopped during runtime due to disk errors.
    // Thus we can't do much here, just mark the service as stopped.
    // Note that this call can't be executed concurrently with any of the
    // operations executed in the disk threads - exec, on_begin/end_io_op
    XLOG_DEBUG(disk_tag,
               "Object_read_handle {}. Obj_key {}. Handling service stopped",
               log_ptr(this), rtrans_.obj_key());
    state_.store(state::service_stopped, std::memory_order_release);
    try_fire_error(cache::service_stopped);
    try_fire_closed(cache::service_stopped);
}

////////////////////////////////////////////////////////////////////////////////

std::pair<bytes32_t, bytes32_t>
object_read_handle::calc_copy_rng(const read_transaction& rtrans,
                                  const range_elem& rng) noexcept
{
    const auto rng_offs = rng.rng_offset();
    const auto trn_offs = rtrans.curr_offset();
    const auto rng_end  = rng.rng_end_offset();
    const auto trn_end = rtrans.end_offset();
    X3ME_ENFORCE(x3me::math::in_range(trn_offs, rng_offs, rng_end),
                 "The range element offset could be smaller for the first "
                 "range or when last time the user provided buffer hasn't "
                 "been big enough and we stayed at the same element. In all "
                 "other cases the offsets must be equal");
    const auto skip = trn_offs - rng_offs;
    const bytes32_t size =
        x3me::math::ranges_overlap(rng_offs, rng_end, trn_offs, trn_end);
    return std::make_pair(skip, size);
}

object_read_handle::read_res
object_read_handle::try_read_all_from_mem_buff() noexcept
{
    if (!frag_read_buff_) // The buffer is lazily allocated i.e. currently empty
        return read_res::end_of_buf;
    if (rtrans_.curr_offset() >= curr_rng_.rng_end_offset())
        return read_res::end_of_buf; // Needs to get a new range.

    // We need to 'steal' the user_data and return it back if not finished.
    // This way we avoid race condition issues with the close functionality.
    user_data::uh_buffers_t ud;
    user_data_->swap_uh_buffers(ud);
    if (ud.empty())
    {
        try_fire_closed(cache::success);
        return read_res::aborted;
    }

    const auto rng    = calc_copy_rng(rtrans_, curr_rng_);
    const auto copied = ud.buffers_.write(x3me::mem_utils::make_array_view(
        frag_data(frag_read_buff_) + rng.first, rng.second));
    rtrans_.inc_read_bytes(copied);

    const auto full = ud.buffers_.all_written();
    const auto fin  = rtrans_.finished();

    XLOG_DEBUG(disk_tag,
               "Try_read_from_mem_buff. Object_read_handle {}. RTrans {}. "
               "Current consumed {}. Buffs_full {}",
               log_ptr(this), rtrans_, copied, full);

    if (!full && !fin)
    {
        // We must not inform the user. We are not done. Restore the user data.
        user_data_->swap_uh_buffers(ud);
        return read_res::end_of_buf;
    }

    err_code_t err;
    if (fin)
    {
        err = err_code_t{cache::eof, get_cache_error_category()};
        read_handle_done();
    }
    ud.handler_(err, ud.buffers_.bytes_written());
    if (fin)
        try_fire_closed(cache::success);

    return read_res::all_read;
}

bool object_read_handle::check_read_data() const noexcept
{
    const auto cur_hdr = frag_hdr(frag_read_buff_);
    const auto exp_hdr =
        object_frag_hdr::create(rtrans_.fs_node_key(), curr_rng_);
    return cur_hdr == exp_hdr;
}

////////////////////////////////////////////////////////////////////////////////

void object_read_handle::read_handle_done() noexcept
{
    fs_ops_->fsmd_end_read(std::move(rtrans_));
    // This ensures that no other operations can be done with this read handle
    state_.store(state::closed, std::memory_order_release);
}

template <typename Err>
void object_read_handle::try_fire_error(Err err) noexcept
{
    user_data::uh_buffers_t ud;
    user_data_->swap_uh_buffers(ud);
    if (ud.handler_)
    {
        ud.handler_(err_code_t{err, get_cache_error_category()}, 0U);
    }
}

template <typename Err>
void object_read_handle::try_fire_closed(Err err) noexcept
{
    if (auto cl_handler = user_data_->release_close_handler())
    {
        if (err == cache::success)
            (*cl_handler)(err_code_t{});
        else
            (*cl_handler)(err_code_t{err, get_cache_error_category()});
    }
}

void object_read_handle::report_disk_error(const err_code_t& err,
                                           bytes64_t off,
                                           bytes32_t len) noexcept
{
    XLOG_FATAL(disk_tag, "Read_operation failed. Disk error "
                         "while reading object fragment. {}. FS "
                         "'{}'. Obj_key {}. Position {}. Size {}",
               err.message(), fs_ops_->vol_path(), rtrans_.obj_key(), off, len);
    fs_ops_->report_disk_error();
}

} // namespace detail
} // namespace cache
