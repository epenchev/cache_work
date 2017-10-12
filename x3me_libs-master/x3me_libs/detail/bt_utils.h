#pragma once

#include <array>
#include <cassert>
#include <cstring>
#include <iterator>
#include <utility>

#include <boost/functional/hash.hpp>

namespace x3me
{
namespace bt_utils
{
namespace detail
{

template <typename T, size_t Size>
class static_mem_view;

// No support for Size = 0
template <typename T, size_t Size>
class byte_array : public std::array<T, Size>
{
public:
    enum
    {
        ssize = Size // ssize to not collide with the size() function
    };

public:
    byte_array() noexcept = default;
    byte_array(const byte_array&) noexcept = default;
    byte_array& operator=(const byte_array&) noexcept = default;
    ~byte_array() noexcept
    {
        // Not very precise check but shorter than explicit checks for all
        // "native" 1 byte types
        // Could be also checked for POD-ness
        static_assert(sizeof(T) == 1,
                      "Works with signed/unsigned char, int8_t, uint8_t");
    }

    // The user needs to ensure that the ih has enough size
    explicit byte_array(const T* p) noexcept { assign(p); }

    explicit byte_array(const static_mem_view<T, Size>& rhs) noexcept
    {
        assign(rhs.data());
    }

    void assign(const T* p) noexcept
    {
        auto& b = this->front();
        assert(p);
        memcpy(&b, p, ssize);
    }

    byte_array& operator=(const static_mem_view<T, Size>& rhs) noexcept
    {
        assign(rhs.data());
        return *this;
    }

    friend bool operator==(const byte_array& lhs,
                           const byte_array& rhs) noexcept
    {
        return (memcmp(lhs.data(), rhs.data(), ssize) == 0);
    }
    friend bool operator<(const byte_array& lhs, const byte_array& rhs) noexcept
    {
        return (memcmp(lhs.data(), rhs.data(), ssize) < 0);
    }
    friend bool operator!=(const byte_array& lhs,
                           const byte_array& rhs) noexcept
    {
        return !(lhs == rhs);
    }
    friend bool operator>(const byte_array& lhs, const byte_array& rhs) noexcept
    {
        return (rhs < lhs);
    }
    friend bool operator<=(const byte_array& lhs,
                           const byte_array& rhs) noexcept
    {
        return !(rhs < lhs);
    }
    friend bool operator>=(const byte_array& lhs,
                           const byte_array& rhs) noexcept
    {
        return !(lhs < rhs);
    }
};

////////////////////////////////////////////////////////////////////////////////

// No support for Size = 0
template <typename T, size_t Size>
class static_mem_view
{
public:
    enum
    {
        ssize = Size // ssize to not collide with the size() function
    };

public:
    typedef T value_type;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

public:
    static_mem_view() noexcept : mem_(nullptr) {}
    explicit static_mem_view(const_pointer m) noexcept : mem_(m) {}
    static_mem_view(const static_mem_view&) = default;
    static_mem_view& operator=(const static_mem_view&) = default;
    ~static_mem_view() noexcept
    {
        // Not very precise check but shorter than explicit checks for all
        // "native" 1 byte types
        // Could be also checked for POD-ness
        static_assert(sizeof(T) == 1,
                      "Works with signed/unsigned char, int8_t, uint8_t");
    }

    void reset(const_pointer m = nullptr) noexcept { mem_ = m; }

    void swap(static_mem_view& rhs) noexcept
    {
        using std::swap;
        swap(mem_, rhs.mem_);
    }

    explicit operator bool() const noexcept { return (mem_ != nullptr); }

    bool empty() const noexcept { return (mem_ == nullptr); }

    const_pointer data() const noexcept { return mem_; }
    static constexpr size_type size() noexcept { return ssize; }

    const_reference operator[](size_type i) const noexcept
    {
        assert(mem_);
        assert(i < ssize);
        return mem_[i];
    }

    const_iterator cbegin() const noexcept
    {
        assert(mem_);
        return mem_;
    }
    const_iterator cend() const noexcept
    {
        assert(mem_);
        return mem_ + ssize;
    }
    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator end() const noexcept { return cend(); }

    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }
    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }
    const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    const_reverse_iterator rend() const noexcept { return crend(); }

    friend bool operator==(const static_mem_view& lhs,
                           const static_mem_view& rhs) noexcept
    {
        auto d1 = lhs.data();
        auto d2 = rhs.data();
        if (d1 && d2)
            return (memcmp(d1, d2, ssize) == 0);
        return (!d1 && !d2);
    }
    friend bool operator<(const static_mem_view& lhs,
                          const static_mem_view& rhs) noexcept
    {
        auto d1 = lhs.data();
        auto d2 = rhs.data();
        if (d1 && d2)
            return (memcmp(d1, d2, ssize) < 0);
        // If d2 is null d1 is not null, so d1 is > than d2
        // and vice versa
        return !!d2;
    }
    friend bool operator!=(const static_mem_view& lhs,
                           const static_mem_view& rhs) noexcept
    {
        return !(lhs == rhs);
    }
    friend bool operator>(const static_mem_view& lhs,
                          const static_mem_view& rhs) noexcept
    {
        return (rhs < lhs);
    }
    friend bool operator<=(const static_mem_view& lhs,
                           const static_mem_view& rhs) noexcept
    {
        return !(rhs < lhs);
    }
    friend bool operator>=(const static_mem_view& lhs,
                           const static_mem_view& rhs) noexcept
    {
        return !(lhs < rhs);
    }

private:
    const_pointer mem_;
};

} // namespace detail
} // namespace bt_utils
} // namespace x3me
