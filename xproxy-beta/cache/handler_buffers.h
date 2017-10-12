#pragma once

namespace cache
{
namespace detail
{
class buffers;

// A small helper class for managing the provided handler and buffers
template <typename Handler, typename Buffers>
struct handler_buffers
{
    Handler handler_;
    Buffers buffers_;

    template <typename Buffs>
    void set(Handler&& h, Buffs&& b) noexcept
    {
        handler_ = std::move(h);
        buffers_ = std::move(b);
    }

    void swap(handler_buffers& rhs) noexcept
    {
        handler_.swap(rhs.handler_);
        buffers_.swap(rhs.buffers_);
    }

    bool empty() const noexcept { return !handler_ && buffers_.empty(); }
};

} // namespace detail
} // namespace cache
