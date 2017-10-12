#pragma once

#include "public_types.h"

namespace net
{

class async_read_stream
{
public:
    struct implementation
    {
        virtual ~implementation() noexcept {}
        virtual void async_read_some(const vec_wr_buffer_t&,
                                     handler_t&&) noexcept = 0;
        virtual void shutdown(asio_shutdown_t, err_code_t&) noexcept = 0;
        virtual void close(err_code_t&) noexcept = 0;
        virtual bool is_open() const noexcept = 0;
    };
    // We can't have explicit template arguments passed to the constructor
    // and thus we need this intermediate type.
    template <typename T>
    struct impl_type
    {
    };

private:
    // Unfortunately we need some kind of type erasure here in order to
    // decouple the net layer, from the actual implementation of the
    // async_read_stream. However, we want to avoid heap allocation and
    // so we use this a bit ugly scheme to do that.
    std::aligned_storage_t<40, alignof(implementation*)> storage_;
    bool valid_;

public:
    async_read_stream() noexcept;
    template <typename Impl, typename... Args>
    async_read_stream(impl_type<Impl>, Args&&... args) noexcept;

    async_read_stream(const async_read_stream& rhs) = delete;
    async_read_stream& operator=(const async_read_stream& rhs) = delete;

    async_read_stream(async_read_stream&& rhs) noexcept;
    async_read_stream& operator=(async_read_stream&& rhs) noexcept;

    ~async_read_stream() noexcept;

    template <typename Handler>
    void async_read_some(const vec_wr_buffer_t& buff, Handler&& h) noexcept
    {
        return impl()->async_read_some(buff,
                                       handler_t{std::forward<Handler>(h)});
    }

    void shutdown(asio_shutdown_t how, err_code_t& err) noexcept
    {
        return impl()->shutdown(how, err);
    }

    void close(err_code_t& err) noexcept { return impl()->close(err); }

    bool is_open() const noexcept { return impl()->is_open(); }

    bool valid() const noexcept { return valid_; }

private:
    implementation* impl() noexcept
    {
        return static_cast<implementation*>(static_cast<void*>(&storage_));
    }
    const implementation* impl() const noexcept
    {
        return static_cast<const implementation*>(
            static_cast<const void*>(&storage_));
    }
};

////////////////////////////////////////////////////////////////////////////////

template <typename Impl, typename... Args>
async_read_stream::async_read_stream(impl_type<Impl>, Args&&... args) noexcept
    : valid_(true)
{
    static_assert(
        std::is_base_of<implementation, Impl>::value,
        "The provided type must be inherited from the 'implementation'");
    static_assert(sizeof(Impl) <= sizeof(storage_),
                  "Can't support so big type");
    static_assert(alignof(Impl) <= alignof(decltype(storage_)),
                  "Can't support such alignment");
    new (&storage_) Impl(std::forward<Args>(args)...);
}

} // namespace net
