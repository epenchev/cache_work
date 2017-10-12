#pragma once

namespace http
{
namespace detail
{

class hdr_value_pos
{
    template <size_t>
    friend class hdr_values_store;

    bytes32_t beg_ = 0;
    bytes32_t end_ = 0;

    hdr_value_pos(bytes32_t b, bytes32_t e) noexcept : beg_(b), end_(e) {}

public:
    hdr_value_pos() noexcept = default;

    bool empty() const noexcept { return end_ == beg_; }

    hdr_value_pos sub_pos(bytes32_t beg, bytes32_t end) const noexcept
    {
        assert(end <= (end_ - beg_));
        assert(beg <= end);
        return hdr_value_pos(beg_ + beg, beg_ + end);
    }
};

} // namespace
} // namespace
