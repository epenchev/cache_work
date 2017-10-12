#include "precompiled.h"
#include "async_cache_reader.h"
#include "cache/cache_error.h"

namespace http
{
namespace detail
{

async_cache_reader::async_cache_reader(cache::async_stream&& h) noexcept
    : cache_handle_(std::move(h))
{
}

async_cache_reader::~async_cache_reader() noexcept
{
}

void async_cache_reader::async_read_some(const net::vec_wr_buffer_t& buff,
                                         net::handler_t&& h) noexcept
{
    using namespace boost::asio;
    cache::mutable_buffers cbuff;
    for (const auto& b : buff)
        cbuff.emplace_back(buffer_cast<uint8_t*>(b), buffer_size(b));

    cache_handle_.async_read(
        std::move(cbuff),
        [h = std::move(h)](const err_code_t& err, bytes32_t read)
        {
            if (err == cache::eof)
                h(asio_error::eof, read);
            else
                h(err, read);
        });
}

void async_cache_reader::shutdown(asio_shutdown_t, err_code_t&) noexcept
{
    // Nothing to do here
}

void async_cache_reader::close(err_code_t&) noexcept
{
    cache_handle_.async_close();
}

bool async_cache_reader::is_open() const noexcept
{
    return cache_handle_.is_open();
}

} // namespace detail
} // namespace http
