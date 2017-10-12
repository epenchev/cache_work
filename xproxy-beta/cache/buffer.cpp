#include "precompiled.h"
#include "buffer.h"

namespace cache
{
namespace detail
{

template <typename Buff>
buffers::buffers(std::initializer_list<Buff> rhs) noexcept
{
    // We know that the container has exactly one element because
    // the function is instantiated only for single element buffers.
    data_.reserve(rhs.size());
    for (auto& r : rhs)
        data_.push_back(r.data_[0]);
}
template buffers::buffers(std::initializer_list<const_buffer>) noexcept;
template buffers::buffers(std::initializer_list<mutable_buffer>) noexcept;

template <typename Buff>
buffers& buffers::operator=(std::initializer_list<Buff> rhs) noexcept
{
    data_.clear();
    // We know that the container has exactly one element because
    // the function is instantiated only for single element buffers.
    data_.reserve(rhs.size());
    for (auto& r : rhs)
        data_.push_back(r.data_[0]);
    return *this;
}
template buffers& buffers::
operator=(std::initializer_list<const_buffer>) noexcept;
template buffers& buffers::
operator=(std::initializer_list<mutable_buffer>) noexcept;

} // namespace detail
} // namespace cache
