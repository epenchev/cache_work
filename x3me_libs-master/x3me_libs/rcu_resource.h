#pragma once

#include <memory>
#include <mutex>
#include <type_traits>

#include "thread_common.h"
#include "spin_lock.h"

namespace x3me
{
namespace thread
{

// The class implements the ideas of the Read-Copy-Update (RCU) technique,
// which is widely used in the Linux kernel in a slightly different manner.
// It also uses the logic behind the upcoming atomic_shared_ptr.
// Concurrent operations on the same shared_ptr instance are not thread safe.
// However, concurrent operations on different shared_ptr instances pointing
// to the same object are safe. There are special atomic operations for
// shared_ptr objects to ensure thread safety, but they are forced to use
// look-aside data structures or global locks, because they are free functions.
// This makes them more inefficient than the upcoming atomic_shared_ptr which
// embeds the needed spin lock inside the given instance.
// The same thing is done in this functionality.
template <typename T>
class rcu_resource
{
public:
    using type          = typename std::add_const<T>::type;
    using resource_type = std::shared_ptr<type>;

private:
    using lock_guard_t = std::lock_guard<spin_lock>;

    resource_type resource_;
    mutable spin_lock lock_;

public:
    rcu_resource() noexcept {}
    template <typename... Args>
    explicit rcu_resource(in_place_t, Args&&... args)
        : resource_(std::make_shared<type>(std::forward<Args>(args)...))
    {
    }

    rcu_resource(const rcu_resource& rhs) noexcept : resource_(rhs.read_copy())
    {
    }

    rcu_resource(rcu_resource&& rhs) noexcept : resource_(rhs.release()) {}

    rcu_resource& operator=(const rcu_resource& rhs) noexcept
    {
        if (this != &rhs)
        {
            update(rhs.read_copy());
        }
        return *this;
    }

    rcu_resource& operator=(rcu_resource&& rhs) noexcept
    {
        if (this != &rhs)
        {
            update(rhs.release());
        }
        return *this;
    }

    resource_type read_copy() const noexcept
    {
        lock_guard_t _(lock_);
        return resource_;
    }

    void update(resource_type r) noexcept
    {
        {
            lock_guard_t _(lock_);
            resource_.swap(r);
        }
        // Do the potential destruction of 'r' outside the critical section
    }

    template <typename... Args>
    void update(in_place_t, Args&&... args) noexcept
    {
        update(std::make_shared<type>(std::forward<Args>(args)...));
    }

    resource_type release() noexcept
    {
        resource_type ret;
        {
            lock_guard_t _(lock_);
            resource_.swap(ret);
        }
        return ret;
    }
};

} // namespace thread
} // namespace x3me
