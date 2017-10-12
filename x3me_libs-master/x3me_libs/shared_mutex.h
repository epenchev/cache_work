#pragma once

#include <pthread.h>
#include "x3me_assert.h"

namespace x3me
{
namespace thread
{

// This functionality is useful when we don't want to link with the
// whole boost thread library just for a shared_mutex.
// This functionality is going to be removed once we have a standard library
// with shared_mutex support.
// Very simple shared_mutex which implements
// 1. BasicLockable concept
// 2. BasicSharedLockable concept - it doesn't exist officially.
// I invented it :)
class shared_mutex
{
    pthread_rwlock_t rw_lock_;

public:
    shared_mutex() noexcept { pthread_rwlock_init(&rw_lock_, nullptr); }
    ~shared_mutex() noexcept { pthread_rwlock_destroy(&rw_lock_); }

    shared_mutex(const shared_mutex&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;
    shared_mutex(shared_mutex&&) = delete;
    shared_mutex& operator=(shared_mutex&&) = delete;

    // BasicLocable concept
    void lock() noexcept
    {
        int res = pthread_rwlock_wrlock(&rw_lock_);
        // Better crash than continue to work with silent race condition.
        X3ME_ENFORCE(res == 0);
    }
    void unlock() noexcept
    {
        int res = pthread_rwlock_unlock(&rw_lock_);
        // Better crash than continue to work with silent race condition.
        X3ME_ENFORCE(res == 0);
    }

    // BasicSharedLockable concept
    void lock_shared() noexcept
    {
        int res = pthread_rwlock_rdlock(&rw_lock_);
        // Better crash than continue to work with silent race condition.
        X3ME_ENFORCE(res == 0);
    }
    void unlock_shared() noexcept
    {
        int res = pthread_rwlock_unlock(&rw_lock_);
        // Better crash than continue to work with silent race condition.
        X3ME_ENFORCE(res == 0);
    }
};

class shared_lock
{
    shared_mutex& mut_;

public:
    explicit shared_lock(shared_mutex& mut) noexcept : mut_(mut)
    {
        mut_.lock_shared();
    }

    ~shared_lock() noexcept { mut_.unlock_shared(); }

    shared_lock(const shared_lock&) = delete;
    shared_lock& operator=(const shared_lock&) = delete;
    shared_lock(shared_lock&&) = delete;
    shared_lock& operator=(shared_lock&&) = delete;
};

// For non-shared locking can be used lock_guard or unique_lock.

} // namespace thread
} // namespace x3me
