#pragma once

#include "cache/async_stream.h"
#include "net/async_read_stream.h"

namespace http
{
namespace detail
{

class async_cache_reader final : public net::async_read_stream::implementation
{
    cache::async_stream cache_handle_;

public:
    explicit async_cache_reader(cache::async_stream&& h) noexcept;
    ~async_cache_reader() noexcept final;

    void async_read_some(const net::vec_wr_buffer_t& buff,
                         net::handler_t&& h) noexcept final;
    void shutdown(asio_shutdown_t, err_code_t&) noexcept final;
    void close(err_code_t&) noexcept final;
    bool is_open() const noexcept final;
};

} // namespace detail
} // namespace http
