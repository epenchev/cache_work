#include "switchable_read_stream.h"

namespace net
{
namespace detail
{

template <typename S1, typename S2>
switchable_read_stream<S1, S2>::switchable_read_stream() noexcept
{
}

template <typename S1, typename S2>
switchable_read_stream<S1, S2>::switchable_read_stream(S1&& rhs) noexcept
    : stream_(std::move(rhs))
{
}

template <typename S1, typename S2>
switchable_read_stream<S1, S2>::switchable_read_stream(S2&& rhs) noexcept
    : stream_(std::move(rhs))
{
}

template <typename S1, typename S2>
switchable_read_stream<S1, S2>::~switchable_read_stream() noexcept
{
}

template <typename S1, typename S2>
switchable_read_stream<S1, S2>& switchable_read_stream<S1, S2>::
operator=(S1&& rhs) noexcept
{
    stream_ = std::move(rhs);
    return *this;
}

template <typename S1, typename S2>
switchable_read_stream<S1, S2>& switchable_read_stream<S1, S2>::
operator=(S2&& rhs) noexcept
{
    stream_ = std::move(rhs);
    return *this;
}

template <typename S1, typename S2>
template <typename Handler>
void switchable_read_stream<S1, S2>::async_read_some(
    const vec_wr_buffer_t& buff, Handler&& h) noexcept
{
    if (auto* s1 = boost::get<S1>(&stream_))
    {
        s1->async_read_some(buff, std::forward<Handler>(h));
    }
    else if (auto* s2 = boost::get<S2>(&stream_))
    {
        s2->async_read_some(buff, std::forward<Handler>(h));
    }
    else
    {
        X3ME_ASSERT(false, "Empty switchable stream");
    }
}

template <typename S1, typename S2>
void switchable_read_stream<S1, S2>::shutdown(asio_shutdown_t how,
                                              err_code_t& err) noexcept
{
    if (auto* s1 = boost::get<S1>(&stream_))
    {
        s1->shutdown(how, err);
    }
    else if (auto* s2 = boost::get<S2>(&stream_))
    {
        s2->shutdown(how, err);
    }
    else
    {
        X3ME_ASSERT(false, "Empty switchable stream");
    }
}

template <typename S1, typename S2>
void switchable_read_stream<S1, S2>::close(err_code_t& err) noexcept
{
    if (auto* s1 = boost::get<S1>(&stream_))
    {
        s1->close(err);
    }
    else if (auto* s2 = boost::get<S2>(&stream_))
    {
        s2->close(err);
    }
    else
    {
        X3ME_ASSERT(false, "Empty switchable stream");
    }
}

template <typename S1, typename S2>
bool switchable_read_stream<S1, S2>::is_open() const noexcept
{
    if (auto* s1 = boost::get<S1>(&stream_))
    {
        return s1->is_open();
    }
    else if (auto* s2 = boost::get<S2>(&stream_))
    {
        return s2->is_open();
    }
    else
    {
        X3ME_ASSERT(false, "Empty switchable stream");
    }
    return false;
}

template <typename S1, typename S2>
template <typename S>
S& switchable_read_stream<S1, S2>::get() noexcept
{
    static_assert(std::is_same<S, S1>::value || std::is_same<S, S2>::value,
                  "Can't get this type");
    return boost::get<S>(stream_); // Let it crash here if it throws
}

template <typename S1, typename S2>
template <typename S>
const S& switchable_read_stream<S1, S2>::get() const noexcept
{
    static_assert(std::is_same<S, S1>::value || std::is_same<S, S2>::value,
                  "Can't get this type");
    return boost::get<S>(stream_); // Let it crash here if it throws
}

template <typename S1, typename S2>
template <typename S>
bool switchable_read_stream<S1, S2>::is() const noexcept
{
    static_assert(std::is_same<S, S1>::value || std::is_same<S, S2>::value,
                  "Can't get check type");
    return (boost::get<S>(&stream_) != nullptr);
}

} // namespace detail
} // namespace net
