#pragma once

#include "aio_task.h"
#include "async_handlers_fwds.h"
#include "cache_fs_ops_fwds.h"
#include "object_key.h"

namespace cache
{
namespace detail
{

class object_open_handle : public aio_task
{
protected:
    cache_fs_ops_ptr_t fs_ops_;
    const object_key obj_key_;

public:
    object_open_handle() = delete;
    object_open_handle(const cache_fs_ops_ptr_t& fso,
                       const object_key& obj_key) noexcept;
    virtual ~object_open_handle() noexcept;

    // Note that it's important the function to be called when the outside
    // have a reference counted copy of the object handle.
    // Otherwise this call could remove the last reference to the object
    // but continue to use the member variables after that.
    virtual void async_close() noexcept = 0;

private:
    aio_op operation() const noexcept final { return aio_op::exec; }

    non_owner_ptr_t<const aio_data> on_begin_io_op() noexcept final;

    void on_end_io_op(const err_code_t&) noexcept final;
};
using object_ohandle_ptr_t = aio_task_ptr_t<object_open_handle>;

////////////////////////////////////////////////////////////////////////////////

class object_open_read_handle final : public object_open_handle
{
    using handler_t = open_rhandler_t;

    handler_t handler_;

public:
    object_open_read_handle(const cache_fs_ops_ptr_t& fso,
                            const object_key& obj_key,
                            handler_t&& h) noexcept;

private:
    void exec() noexcept final;

    void service_stopped() noexcept final;

    void async_close() noexcept final;
};

////////////////////////////////////////////////////////////////////////////////

class object_open_write_handle final : public object_open_handle
{
    using handler_t = open_whandler_t;

    handler_t handler_;
    const bool truncate_object_;

public:
    object_open_write_handle(const cache_fs_ops_ptr_t& fso,
                             const object_key& obj_key,
                             bool truncate_object,
                             handler_t&& h) noexcept;

private:
    void exec() noexcept final;

    void service_stopped() noexcept final;

    void async_close() noexcept final;
};

} // namespace detail
} // namespace cache
