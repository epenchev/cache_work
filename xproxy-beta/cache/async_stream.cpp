#include "precompiled.h"
#include "async_stream.h"
#include "cache_error.h"
#include "object_distributor.h"

namespace cache
{

async_stream::async_stream(object_distributor& od,
                           io_service_t& handler_ios) noexcept
    : obj_distributor_(&od),
      handler_ios_(&handler_ios),
      // Just put something because of never empty guarantee
      handle_(detail::object_ohandle_ptr_t{})
{
}

async_stream::~async_stream() noexcept
{
    X3ME_ASSERT(!op_in_progress() && !cl_in_progress(),
                "Destroying this object while there is "
                "asynchronous operation on it means that you "
                "don't keep it alive during the operation. "
                "This can lead to dangling references");
    // It's valid the object handle to not be closed explicitly
    // as far as there is no asynchronous operation in progress.
    async_close();
}

async_stream::async_stream(async_stream&& rhs) noexcept
    : obj_distributor_(rhs.obj_distributor_),
      handler_ios_(rhs.handler_ios_),
      handle_(std::move(rhs.handle_))
{
    X3ME_ASSERT(!rhs.op_in_progress() && !cl_in_progress(),
                "Moving objects while there is asynchronous operation in "
                "progress is not allowed");
    // The obj_distributor_ and the handler_ios_ of the rhs
    // are not zeroed because we want the rhs object to be reusable
}

async_stream& async_stream::operator=(async_stream&& rhs) noexcept
{
    X3ME_ASSERT(!op_in_progress() && !rhs.op_in_progress() &&
                    !cl_in_progress() && !rhs.cl_in_progress(),
                "Moving objects while there is asynchronous operation in "
                "progress is not allowed");
    if (X3ME_LIKELY(this != &rhs))
    {
        obj_distributor_ = rhs.obj_distributor_;
        handler_ios_     = rhs.handler_ios_;
        handle_          = std::move(rhs.handle_);
    }
    return *this;
}

void async_stream::async_close() noexcept
{
    struct closer : boost::static_visitor<void>
    {
        void operator()(detail::object_ohandle_ptr_t& h) const noexcept
        {
            if (h)
            {
                h->async_close();
                h.reset();
            }
        }
        void operator()(detail::object_rhandle_ptr_t& h) const noexcept
        {
            if (h)
            {
                h->async_close();
                h.reset();
            }
        }
        void operator()(detail::object_whandle_ptr_t& h) const noexcept
        {
            if (h)
            {
                h->async_close();
                h.reset();
            }
        }
    };
    boost::apply_visitor(closer{}, handle_);
}

bool async_stream::is_open() const noexcept
{
    struct checker : boost::static_visitor<bool>
    {
        bool operator()(const detail::object_ohandle_ptr_t&) const noexcept
        {
            return false;
        }
        bool operator()(const detail::object_rhandle_ptr_t& h) const noexcept
        {
            return !!h;
        }
        bool operator()(const detail::object_whandle_ptr_t& h) const noexcept
        {
            return !!h;
        }
    };
    return boost::apply_visitor(checker{}, handle_);
}

////////////////////////////////////////////////////////////////////////////////

void async_stream::async_open_read_impl(const cache_key& key,
                                        bytes64_t skip_bytes,
                                        open_rhandler_t&& h) noexcept
{
    if (auto handle =
            obj_distributor_->async_open_read(key, skip_bytes, std::move(h)))
    {
        handle_ = std::move(handle);
    }
    else
        h(cache::tasks_limit_reached, detail::object_rhandle_ptr_t{});
}

void async_stream::async_open_write_impl(const cache_key& key,
                                         bool truncate_object,
                                         open_whandler_t&& h) noexcept
{
    if (auto handle = obj_distributor_->async_open_write(key, truncate_object,
                                                         std::move(h)))
    {
        handle_ = std::move(handle);
    }
    else
        h(cache::tasks_limit_reached, detail::object_whandle_ptr_t{});
}

////////////////////////////////////////////////////////////////////////////////

err_code_t async_stream::err_already_open() noexcept
{
    return err_code_t(cache::already_open, get_cache_error_category());
}

err_code_t async_stream::err_invalid_handle() noexcept
{
    return err_code_t(cache::invalid_handle, get_cache_error_category());
}

} // namespace cache
