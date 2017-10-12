#pragma once

#include "object_open_handle.h"
#include "object_read_handle.h"
#include "object_write_handle.h"

namespace cache
{
struct cache_key;
struct mutable_buffer;
struct mutable_buffers;
struct const_buffer;
struct const_buffers;
struct object_distributor;

namespace detail
{
class buffers;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////
// Object of this class is not safe to be used by multiple threads.
// It's undefined behavior if the caller modifies the passed buffers
// before the async callback gets called.
// The object assumes that the passed handler io_service is running
// in a single thread and takes advantage of this.
// TODO It's unclear currently if the timeout will come from outside, from the
// async_stream user, or from the inside (from the cache_fs functionality).
// We'll need a mechanics to distinguish between a canceled/aborted/timedout
// async_task and a closed one. It the latter case we'll probably need to flush
// store the data that we have so far.
class async_stream
{
    object_distributor* obj_distributor_;
    x3me::mem_utils::tagged_ptr<io_service_t> handler_ios_;
    // This object_handle is passed between threads and having it on the heap
    // makes the code easier to reason about it's multi-threading behavior.
    using handle_t = boost::variant<detail::object_ohandle_ptr_t,
                                    detail::object_rhandle_ptr_t,
                                    detail::object_whandle_ptr_t>;
    handle_t handle_;

public:
    async_stream(object_distributor& od, io_service_t& handler_ios_) noexcept;
    ~async_stream() noexcept;

    // It's illegal to move stream object while there is async_operation
    // started on it.
    async_stream(async_stream&& rhs) noexcept;
    async_stream& operator=(async_stream&& rhs) noexcept;

    async_stream(const async_stream&) = delete;
    async_stream& operator=(const async_stream&) = delete;

    // TODO Document the errors that the handlers may return.
    // The user may have the beginning of the data and thus it may want to
    // skip some bytes at the beginning. Note that the skip_bytes must not be
    // bigger than the requested length.
    template <typename Handler>
    void async_open_read(const cache_key& key,
                         bytes64_t skip_bytes,
                         Handler&& h) noexcept;

    template <typename Handler>
    void async_open_write(const cache_key& key,
                          bool truncate_object,
                          Handler&& h) noexcept;

    template <typename MutableBuffers, typename Handler>
    void async_read(MutableBuffers&& bufs, Handler&& h) noexcept;

    template <typename ConstBuffers, typename Handler>
    void async_write(ConstBuffers&& bufs, Handler&& h) noexcept;

    // The difference between two async_close calls is that the second call
    // will call the current async_operation handler with operation aborted
    // if there is a pending operation and it succeed to interrupt it.
    // The first call will do the above operations too, but it'll call the
    // provided async handler when the all of the close operations are done.
    template <typename Handler>
    void async_close(Handler&& h) noexcept;
    void async_close() noexcept;

    bool is_open() const noexcept;

private:
    using open_rhandler_t =
        std::function<void(const err_code_t&, detail::object_rhandle_ptr_t&&)>;
    using open_whandler_t =
        std::function<void(const err_code_t&, detail::object_whandle_ptr_t&&)>;

    // Functions used to decrease the code in the header
    void async_open_read_impl(const cache_key& key,
                              bytes64_t skip_bytes,
                              open_rhandler_t&& h) noexcept;
    void async_open_write_impl(const cache_key& key,
                               bool truncate_object,
                               open_whandler_t&& h) noexcept;

    // These are currently used only to assert proper usage.
    // Could be removed in the future.
    bool op_in_progress() const noexcept { return handler_ios_.tag_bit<0>(); }
    void set_op_in_progress(bool v) noexcept { handler_ios_.set_tag_bit<0>(v); }
    bool cl_in_progress() const noexcept { return handler_ios_.tag_bit<1>(); }
    void set_cl_in_progress(bool v) noexcept { handler_ios_.set_tag_bit<1>(v); }

    // Used to avoid the inclusion of the cache error in the header
    static err_code_t err_already_open() noexcept;
    static err_code_t err_invalid_handle() noexcept;
};

////////////////////////////////////////////////////////////////////////////////

template <typename Handler>
void async_stream::async_open_read(const cache_key& key,
                                   bytes64_t skip_bytes,
                                   Handler&& h) noexcept
{
    static_assert(
        std::is_same<decltype(h(*(const err_code_t*)nullptr)), void>::value,
        "The open_read handler must be 'void (const err_code_t&)'");
    if (is_open())
    {
        handler_ios_->post([h = std::forward<Handler>(h)]
                           {
                               h(err_already_open());
                           });
        return;
    }
    X3ME_ASSERT(!op_in_progress(),
                "Multiple async operations in progress are not allowed");
    set_op_in_progress(true);
    async_open_read_impl(
        key, skip_bytes,
        [ this, h = std::forward<Handler>(h) ](
            const err_code_t& err, detail::object_rhandle_ptr_t&& oh) mutable
        {
            handler_ios_->post(
                [ this, h = std::move(h), oh = std::move(oh), err ]() mutable
                {
                    handle_ = std::move(oh);
                    set_op_in_progress(false);
                    h(err);
                });
        });
}

template <typename Handler>
void async_stream::async_open_write(const cache_key& key,
                                    bool truncate_object,
                                    Handler&& h) noexcept
{
    static_assert(
        std::is_same<decltype(h(*(const err_code_t*)nullptr)), void>::value,
        "The open_write handler must be 'void (const err_code_t&)'");
    if (is_open())
    {
        handler_ios_->post([h = std::forward<Handler>(h)]
                           {
                               h(err_already_open());
                           });
        return;
    }
    X3ME_ASSERT(!op_in_progress(),
                "Multiple async operations in progress are not allowed");
    set_op_in_progress(true);
    async_open_write_impl(
        key, truncate_object,
        [ this, h = std::forward<Handler>(h) ](
            const err_code_t& err, detail::object_whandle_ptr_t&& oh) mutable
        {
            handler_ios_->post(
                [ this, h = std::move(h), oh = std::move(oh), err ]() mutable
                {
                    handle_ = std::move(oh);
                    set_op_in_progress(false);
                    h(err);
                });
        });
}

template <typename MutableBuffers, typename Handler>
void async_stream::async_read(MutableBuffers&& buff, Handler&& h) noexcept
{
    static_assert(
        std::is_same<decltype(buff), mutable_buffer&&>::value ||
            std::is_same<decltype(buff), mutable_buffers&&>::value,
        "The function expects rvalue reference to detail::mutable_buffer or "
        "mutable_buffers");
    static_assert(
        std::is_same<decltype(h(*(const err_code_t*)nullptr, 0U)), void>::value,
        "The read handler must be 'void (const err_code_t&, uint32_t)'");
    auto* obj_handle = boost::get<detail::object_rhandle_ptr_t>(&handle_);
    if (!obj_handle || !obj_handle->get())
    {
        handler_ios_->post([h = std::forward<Handler>(h)]
                           {
                               h(err_invalid_handle(), 0U);
                           });
        return;
    }
    X3ME_ASSERT(!op_in_progress(),
                "Multiple async operations in progress are not allowed");
    set_op_in_progress(true);
    (*obj_handle)
        ->async_read(
            std::move(buff), [ this, h = std::forward<Handler>(h) ](
                                 const err_code_t& err, uint32_t bytes) mutable
            {
                handler_ios_->post([ this, h = std::move(h), err, bytes ]
                                   {
                                       set_op_in_progress(false);
                                       h(err, bytes);
                                   });
            });
}

template <typename ConstBuffers, typename Handler>
void async_stream::async_write(ConstBuffers&& buff, Handler&& h) noexcept
{
    static_assert(
        std::is_same<decltype(buff), const_buffer&&>::value ||
            std::is_same<decltype(buff), const_buffers&&>::value,
        "The function expects rvalue reference to detail::const_buffer or "
        "const_buffers");
    static_assert(
        std::is_same<decltype(h(*(const err_code_t*)nullptr, 0U)), void>::value,
        "The write handler must be 'void (const err_code_t&, uint32_t)'");
    auto* obj_handle = boost::get<detail::object_whandle_ptr_t>(&handle_);
    if (!obj_handle || !obj_handle->get())
    {
        handler_ios_->post([h = std::forward<Handler>(h)]
                           {
                               h(err_invalid_handle(), 0U);
                           });
        return;
    }
    X3ME_ASSERT(!op_in_progress(),
                "Multiple async operations in progress are not allowed");
    set_op_in_progress(true);
    (*obj_handle)
        ->async_write(
            std::move(buff), [ this, h = std::forward<Handler>(h) ](
                                 const err_code_t& err, uint32_t bytes) mutable
            {
                handler_ios_->post([ this, h = std::move(h), err, bytes ]
                                   {
                                       set_op_in_progress(false);
                                       h(err, bytes);
                                   });
            });
}

template <typename Handler>
void async_stream::async_close(Handler&& h) noexcept
{
    static_assert(
        std::is_same<decltype(h(*(const err_code_t*)nullptr)), void>::value,
        "The close handler must be 'void (const err_code_t&)'");
    auto* obj_handle = boost::get<detail::object_rhandle_ptr_t>(&handle_);
    X3ME_ENFORCE(obj_handle,
                 "The operation is currently supported only for read handles");
    if (!obj_handle->get())
    {
        handler_ios_->post([h = std::forward<Handler>(h)]
                           {
                               h(err_invalid_handle());
                           });
        return;
    }
    set_cl_in_progress(true);
    (*obj_handle)
        ->async_close([ this, h = std::forward<Handler>(h) ](
            const err_code_t& err) mutable
                      {
                          handler_ios_->post([ this, h = std::move(h), err ]
                                             {
                                                 set_cl_in_progress(false);
                                                 h(err);
                                             });
                      });
    obj_handle->reset();
}

} // namespace cache
