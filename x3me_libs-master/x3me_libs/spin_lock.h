#pragma once

namespace x3me
{
namespace thread
{

// Implements mutex concept
class spin_lock
{
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock()
    {
        while (flag_.test_and_set(std::memory_order_acquire))
        {
        }
    }

    bool try_lock() { return !flag_.test_and_set(std::memory_order_acquire); }

    void unlock() { flag_.clear(std::memory_order_release); }
};

} // namespace thread
} // namespace x3me
