#pragma once

#include <ostream>

#include "x3me_assert.h"

namespace x3me
{
namespace mem_utils
{

template <typename T>
class tagged_ptr
{
    static constexpr auto calc_tag_bits(size_t align) noexcept // Basically log2
    {
        uint8_t ret = 0;
        for (; align > 1; align /= 2)
            ++ret;
        return ret;
    }

public:
    using pointer      = T*;
    using element_type = T;
    using tag_type     = uint8_t;

    static constexpr uint8_t tag_bits = calc_tag_bits(alignof(element_type));

private:
    static constexpr uintptr_t tag_mask = alignof(element_type) - 1;

    pointer ptr_ = nullptr;

public:
    tagged_ptr() noexcept = default;
    explicit tagged_ptr(pointer p, tag_type t = 0) NOEXCEPT_ON_X3ME_ASSERT
    {
        reset(p, t);
    }
    tagged_ptr(const tagged_ptr&) noexcept = default;
    tagged_ptr& operator=(const tagged_ptr&) noexcept = default;
    tagged_ptr(tagged_ptr&&) noexcept = default;
    tagged_ptr& operator=(tagged_ptr&&) noexcept = default;
    ~tagged_ptr() noexcept = default;

    pointer get() const noexcept
    {
        return reinterpret_cast<pointer>(reinterpret_cast<uintptr_t>(ptr_) &
                                         ~tag_mask);
    }

    pointer operator->() const noexcept { return get(); }

    element_type& operator*() const noexcept { return *get(); }

    explicit operator bool() const noexcept { return !!get(); }

    void reset(pointer p = nullptr, tag_type t = 0) NOEXCEPT_ON_X3ME_ASSERT
    {
        X3ME_ASSERT((reinterpret_cast<uintptr_t>(p) & tag_mask) == 0,
                    "The given pointer is not correctly aligned");
        ptr_ = p;
        reset_tag(t);
    }

    tag_type get_tag() const noexcept
    {
        return static_cast<tag_type>(reinterpret_cast<uintptr_t>(ptr_) &
                                     tag_mask);
    }

    void reset_tag(tag_type t) NOEXCEPT_ON_X3ME_ASSERT
    {
        X3ME_ASSERT((t & tag_mask) == t, "Tag too large");
        auto p = reinterpret_cast<uintptr_t>(ptr_);
        ptr_   = reinterpret_cast<pointer>((p & ~tag_mask) | (t & tag_mask));
    }

    template <uint8_t bit>
    bool tag_bit() const noexcept
    {
        static_assert(bit < tag_bits, "The big bit number");
        const uintptr_t m = (1 << bit) & tag_mask;
        return (reinterpret_cast<uintptr_t>(ptr_) & m);
    }

    template <uint8_t bit>
    void set_tag_bit(bool v) noexcept
    {
        static_assert(bit < tag_bits, "The big bit number");
        uintptr_t m = (1 << bit) & tag_mask;
        auto p      = reinterpret_cast<uintptr_t>(ptr_);
        ptr_        = reinterpret_cast<pointer>(v ? (p | m) : (p & ~m));
    }

    void swap(tagged_ptr& rhs) noexcept
    {
        auto tmp = ptr_;
        ptr_     = rhs.ptr_;
        rhs.ptr_ = tmp;
    }
};

template <typename T>
constexpr uint8_t tagged_ptr<T>::tag_bits;

////////////////////////////////////////////////////////////////////////////////

template <typename T, typename TG = uint8_t>
auto make_tagged_ptr(T* p, TG tg = 0) noexcept
{
    return tagged_ptr<T>(p, tg);
}

template <typename T>
void swap(tagged_ptr<T>& p1, tagged_ptr<T>& p2) noexcept
{
    p1.swap(p2);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, tagged_ptr<T> p) noexcept
{
    return os << p.get();
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool operator==(tagged_ptr<T> lhs, tagged_ptr<T> rhs) noexcept
{
    return lhs.get() == rhs.get();
}

template <typename T>
bool operator!=(tagged_ptr<T> lhs, tagged_ptr<T> rhs) noexcept
{
    return lhs.get() != rhs.get();
}

template <typename T>
bool operator<(tagged_ptr<T> lhs, tagged_ptr<T> rhs) noexcept
{
    return lhs.get() < rhs.get();
}

template <typename T>
bool operator>(tagged_ptr<T> lhs, tagged_ptr<T> rhs) noexcept
{
    return lhs.get() > rhs.get();
}

template <typename T>
bool operator<=(tagged_ptr<T> lhs, tagged_ptr<T> rhs) noexcept
{
    return lhs.get() <= rhs.get();
}

template <typename T>
bool operator>=(tagged_ptr<T> lhs, tagged_ptr<T> rhs) noexcept
{
    return lhs.get() >= rhs.get();
}

} // namespace mem_utils
} // namespace x3me
