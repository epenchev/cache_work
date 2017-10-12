#include "precompiled.h"
#include "object_open_handle.h"
#include "cache_error.h"
#include "cache_fs_ops.h"
#include "object_read_handle.h"
#include "object_write_handle.h"
#include "read_transaction.h"
#include "write_transaction.h"

namespace cache
{
namespace detail
{

template <typename Handler, typename ObjHandle>
static void
call_reset_handler(Handler& h, const err_code_t& err, ObjHandle&& oh) noexcept
{
    using std::swap;
    Handler hh;
    swap(hh, h);
    hh(err, std::forward<ObjHandle>(oh));
}

////////////////////////////////////////////////////////////////////////////////

object_open_handle::object_open_handle(const cache_fs_ops_ptr_t& fso,
                                       const object_key& obj_key) noexcept
    : fs_ops_(fso),
      obj_key_(obj_key)
{
}

object_open_handle::~object_open_handle() noexcept
{
    XLOG_DEBUG(disk_tag, "Object_open_handle {} destroy. Obj_key {}",
               log_ptr(this), obj_key_);
}

non_owner_ptr_t<const aio_data> object_open_handle::on_begin_io_op() noexcept
{
    X3ME_ASSERT(false, "We don't do IO when open handle");
    return nullptr;
}

void object_open_handle::on_end_io_op(const err_code_t&) noexcept
{
    X3ME_ASSERT(false, "We don't do IO when open handle");
}

////////////////////////////////////////////////////////////////////////////////

object_open_read_handle::object_open_read_handle(const cache_fs_ops_ptr_t& fso,
                                                 const object_key& obj_key,
                                                 handler_t&& h) noexcept
    : object_open_handle(fso, obj_key),
      handler_(std::move(h))
{
    XLOG_DEBUG(disk_tag, "Object_open_handle {} created for read. Obj_key {}",
               log_ptr(this), obj_key_);
}

void object_open_read_handle::exec() noexcept
{
    if (auto read_trans = fs_ops_->fsmd_begin_read(obj_key_))
    {
        auto orh =
            make_aio_task<object_read_handle>(fs_ops_, std::move(read_trans));
        call_reset_handler(handler_, err_code_t{}, std::move(orh));
    }
    else
    {
        XLOG_DEBUG(disk_tag, "Object_open_handle {}. Not all data available "
                             "for reading. Obj_key {}",
                   log_ptr(this), obj_key_);
        const err_code_t err(cache::object_not_present,
                             get_cache_error_category());
        call_reset_handler(handler_, err, object_rhandle_ptr_t{});
    }
}

void object_open_read_handle::service_stopped() noexcept
{
    // This call can potentially be executed in parallel with the async_close
    // call. This call is executed in some of the disk threads and
    // the async_close call is executed from some of the network threads.
    // However, the aios_cancel_task_read_queue can't succeed if the service
    // gets stopped. The call acts like a barrier and ensures the only
    // one of the functions is actually executed.
    const err_code_t err(cache::service_stopped, get_cache_error_category());
    call_reset_handler(handler_, err, object_rhandle_ptr_t{});
}

void object_open_read_handle::async_close() noexcept
{
    if (fs_ops_->aios_cancel_task_read_queue(this))
    {
        XLOG_DEBUG(disk_tag, "Object_open_handle {}. Canceled. Obj_key {}",
                   log_ptr(this), obj_key_);
        const err_code_t err(cache::operation_aborted,
                             get_cache_error_category());
        call_reset_handler(handler_, err, object_rhandle_ptr_t{});
    }
    else
    {
        XLOG_DEBUG(disk_tag,
                   "Object_open_handle {}. Missed to cancel. Obj_key {}",
                   log_ptr(this), obj_key_);
    }
}

////////////////////////////////////////////////////////////////////////////////

object_open_write_handle::object_open_write_handle(
    const cache_fs_ops_ptr_t& fso,
    const object_key& obj_key,
    bool truncate_object,
    handler_t&& h) noexcept : object_open_handle(fso, obj_key),
                              handler_(std::move(h)),
                              truncate_object_(truncate_object)
{
    XLOG_DEBUG(
        disk_tag,
        "Object_open_handle {} created for write. Obj_key {}. Truncate {}",
        log_ptr(this), obj_key_, truncate_object);
}

void object_open_write_handle::exec() noexcept
{
    if (auto ret = fs_ops_->fsmd_begin_write(obj_key_, truncate_object_))
    {
        auto owh = make_aio_task<object_write_handle>(
            fs_ops_, obj_key_.get_range(), std::move(ret.value()));
        call_reset_handler(handler_, err_code_t{}, std::move(owh));
    }
    else
    {
        XLOG_DEBUG(disk_tag,
                   "Object_open_handle {}. Obj_key {}. Skip write. {}",
                   log_ptr(this), obj_key_, ret.error());
        call_reset_handler(handler_, ret.error(), object_whandle_ptr_t{});
    }
}

void object_open_write_handle::service_stopped() noexcept
{
    // This call can potentially be executed in parallel with the async_close
    // call. This call is executed in some of the disk threads and
    // the async_close call is executed from some of the network threads.
    // However, the aios_cancel_task_read_queue can't succeed if the service
    // gets stopped. The call acts like a barrier and ensures the only
    // one of the functions is actually executed.
    const err_code_t err(cache::service_stopped, get_cache_error_category());
    call_reset_handler(handler_, err, object_whandle_ptr_t{});
}

void object_open_write_handle::async_close() noexcept
{
    if (fs_ops_->aios_cancel_task_read_queue(this))
    {
        XLOG_DEBUG(disk_tag, "Object_open_handle {}. Canceled. Obj_key {}",
                   log_ptr(this), obj_key_);
        const err_code_t err(cache::operation_aborted,
                             get_cache_error_category());
        call_reset_handler(handler_, err, object_whandle_ptr_t{});
    }
    else
    {
        XLOG_DEBUG(disk_tag,
                   "Object_open_handle {}. Missed to cancel. Obj_key {}",
                   log_ptr(this), obj_key_);
    }
}

} // namespace detail
} // namespace cache
