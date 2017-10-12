#pragma once

#include "public_types.h"

namespace net
{
namespace detail
{

template <typename S1, typename S2>
class switchable_read_stream
{
    boost::variant<boost::blank, S1, S2> stream_;

public:
    switchable_read_stream() noexcept;
    explicit switchable_read_stream(S1&& rhs) noexcept;
    explicit switchable_read_stream(S2&& rhs) noexcept;
    ~switchable_read_stream() noexcept;

    // Not needed currently
    switchable_read_stream(const switchable_read_stream&) = delete;
    switchable_read_stream& operator=(const switchable_read_stream&) = delete;
    switchable_read_stream(switchable_read_stream&&) = delete;
    switchable_read_stream& operator=(switchable_read_stream&&) = delete;

    switchable_read_stream& operator=(S1&& rhs) noexcept;
    switchable_read_stream& operator=(S2&& rhs) noexcept;

    template <typename Handler>
    void async_read_some(const vec_wr_buffer_t& buff, Handler&& h) noexcept;

    void shutdown(asio_shutdown_t how, err_code_t& err) noexcept;

    void close(err_code_t& err) noexcept;

    bool is_open() const noexcept;

    template <typename S>
    S& get() noexcept;
    template <typename S>
    const S& get() const noexcept;

    template <typename S>
    bool is() const noexcept;
};

} // namespace detail
} // namespace net
