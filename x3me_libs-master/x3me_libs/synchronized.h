#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include "locked.h"
#include "mpl.h"
#include "thread_common.h"
#include "x3me_assert.h"

namespace x3me
{
namespace thread
{
namespace detail
{
X3ME_DEFINE_HAS_METHOD(lock);
X3ME_DEFINE_HAS_METHOD(unlock);
X3ME_DEFINE_HAS_METHOD(lock_shared);
X3ME_DEFINE_HAS_METHOD(unlock_shared);

} // namespace detail

// Using templates in order to handle
// both const and non-const versions with one function.
// The function can be used when we want to use synchronized for multiple
// operations and keep it locked for all of them instead for every single
// one of them.
template <typename S, typename Func, typename... Args>
decltype(auto) with_synchronized(S& s, Func&& func, Args&&... args)
{
    auto guard = s.operator->();
    return func(*guard, std::forward<Args>(args)...);
}

// The function is used to ensure proper locking order when we work with
// two synchronized objects at the same time. It helps avoiding deadlock
// situations when we work with two synchronized objects at the same time.
template <typename S1, typename S2, typename Func, typename... Args>
decltype(auto) with_synchronized(S1& s1, S2& s2, Func&& func, Args&&... args)
{
    const void* a1 = std::addressof(s1);
    const void* a2 = std::addressof(s2);
    // For unrelated objects we need to use 'std::less' because, according to
    // the standard, the behavior of 'operator <' is undefined.
    // The behavior of 'operator <' is defined only for objects from a
    // single array and few other cases.
    std::less<> cmp;
    if (cmp(a1, a2))
    {
        auto guard1 = s1.operator->();
        auto guard2 = s2.operator->();
        return func(*guard1, *guard2, std::forward<Args>(args)...);
    }
    X3ME_ASSERT(cmp(a2, a1), "The pointers must not point to the same object");
    auto guard2 = s2.operator->();
    auto guard1 = s1.operator->();
    return func(*guard1, *guard2, std::forward<Args>(args)...);
}

////////////////////////////////////////////////////////////////////////////////

// Encapsulates an object paired with its mutex.
// It makes improper usage, in thread unsafe manner, virtually impossible.
// TODO Would be good if the class was no except but expressing the noexcept
// dependency from the noexceptness of the Data and the Mutex made code
// utterly verbose. So, I removed the noexcept for now.
template <typename Data, typename Mutex>
class synchronized
{
    static_assert(X3ME_HAS_METHOD_NMSP(detail, Mutex, lock, void()), "");
    static_assert(X3ME_HAS_METHOD_NMSP(detail, Mutex, unlock, void()), "");

    static constexpr bool is_shared_lockable =
        X3ME_HAS_METHOD_NMSP(detail, Mutex, lock_shared, void()) &&
        X3ME_HAS_METHOD_NMSP(detail, Mutex, unlock_shared, void());

public:
    using locked_t = unique_locked<Data, Mutex>;
    using const_locked_t =
        typename std::conditional<is_shared_lockable,
                                  shared_locked<Data, Mutex>, locked_t>::type;

private:
    Data data_;
    mutable Mutex mut_;

public:
    synchronized() = default;

    synchronized(const synchronized& rhs) : data_(*rhs.operator->()) {}
    synchronized(synchronized&& rhs) : data_(std::move(*rhs.operator->())) {}
    explicit synchronized(const Data& rhs) : data_(rhs) {}
    explicit synchronized(Data&& rhs) : data_(std::move(rhs)) {}
    template <typename... Args>
    explicit synchronized(in_place_t, Args&&... args)
        : data_(std::forward<Args>(args)...)
    {
    }

    synchronized& operator=(const synchronized& rhs)
    {
        if (this != &rhs)
        {
            with_synchronized(*this, rhs, [](Data& lhs, const Data& rhs)
                              {
                                  lhs = rhs;
                              });
        }
        return *this;
    }
    synchronized& operator=(synchronized&& rhs)
    {
        if (this != &rhs)
        {
            with_synchronized(*this, rhs, [](Data& lhs, Data& rhs)
                              {
                                  lhs = std::move(rhs);
                              });
        }
        return *this;
    }
    synchronized& operator=(const Data& rhs)
    {
        auto guard = operator->();
        data_      = rhs;
        return *this;
    }
    synchronized& operator=(Data&& rhs)
    {
        auto guard = operator->();
        data_      = std::move(rhs);
        return *this;
    }

    void swap(synchronized& rhs)
    {
        if (this != &rhs)
        {
            with_synchronized(*this, rhs, [](Data& lhs, Data& rhs)
                              {
                                  using std::swap;
                                  swap(lhs, rhs);
                              });
        }
    }
    void swap(Data& rhs)
    {
        auto guard = operator->();
        using std::swap;
        swap(data_, rhs);
    }

    Data copy() const
    {
        auto guard = operator->();
        return data_;
    }

    locked_t operator->() { return locked_t(data_, mut_); }
    const_locked_t operator->() const { return const_locked_t(data_, mut_); }

    // Useful when you have a mutable object, but want to use the
    // const methods which return const_locked_t object.
    const synchronized& as_const() const noexcept { return *this; }

    friend std::ostream& operator<<(std::ostream& os,
                                    const synchronized& rhs) noexcept
    {
        auto guard = rhs.operator->();
        return os << rhs.data_;
    }
};

template <typename Data, typename Mutex>
void swap(synchronized<Data, Mutex>& lhs, synchronized<Data, Mutex>& rhs)
{
    lhs.swap(rhs);
}

} // namespace thread
} // namespace x3me
