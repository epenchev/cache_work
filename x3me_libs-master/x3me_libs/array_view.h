#pragma once

#include <cassert>
#include <iterator>
#include <type_traits>

#include "utils.h"

namespace x3me
{
namespace mem_utils
{

template <typename T>
class array_view;

namespace detail
{

template <typename T>
struct is_array_view : std::false_type
{
};

template <typename T>
struct is_array_view<array_view<T>> : std::true_type
{
};

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class array_view
{
public:
    using value_type             = T;
    using reference              = T &;
    using pointer                = T *;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using iterator               = T *;
    using const_iterator         = typename std::add_const<T>::type *;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    pointer data_;
    size_type size_;

public:
    constexpr array_view() noexcept : data_(nullptr), size_(0) {}
    constexpr array_view(pointer data, size_type size) noexcept : data_(data),
                                                                  size_(size)
    {
        assert(((size_ > 0) && data_) || (size_ == 0));
    }
    // The constructor is intentionally implicit.
    // It will compile only if the container has data() and size() members
    // or it's an C array i.e. these are containers with contiguous memory.
    // The constructor can also be used for conversion from
    // array_view<T> to array_view<const T> but not vice versa.
    template <typename Cont,
              typename = std::enable_if_t<!detail::is_array_view<Cont>::value>>
    constexpr array_view(Cont &cont) noexcept : data_(utils::data(cont)),
                                                size_(utils::size(cont))
    {
    }

    array_view(const array_view &) noexcept = default;
    array_view &operator=(const array_view &) noexcept = default;
    array_view(array_view &&) noexcept = default;
    array_view &operator=(array_view &&) noexcept = default;

    constexpr explicit operator bool() const noexcept { return (size_ > 0); }

    constexpr bool empty() const noexcept { return !(size_ > 0); }

    constexpr pointer data() const noexcept { return data_; }
    constexpr size_type size() const noexcept { return size_; }

    constexpr reference operator[](size_type i) const noexcept
    {
        // assert(i < size_);
        return data_[i];
    }

    constexpr reference front() const noexcept
    {
        assert(size_ > 0);
        return data_[0];
    }
    constexpr reference back() const noexcept
    {
        assert(size_ > 0);
        return data_[size_ - 1];
    }

    constexpr const_iterator cbegin() const noexcept { return data_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }
    constexpr iterator begin() const noexcept { return data_; }
    constexpr iterator end() const noexcept { return data_ + size_; }

    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }
    constexpr reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    constexpr reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }
};

template <typename T>
constexpr auto make_array_view(T *p, size_t s) noexcept
{
    return array_view<T>(p, s);
}

template <typename Cont>
constexpr auto make_array_view(Cont &cont) noexcept
{
    using vt = typename std::conditional<std::is_const<Cont>::value,
                                         const typename Cont::value_type,
                                         typename Cont::value_type>::type;
    return array_view<vt>(cont);
}

} // namespace mem_utils
} // namespace x3me
