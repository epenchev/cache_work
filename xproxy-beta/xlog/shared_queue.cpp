#include "precompiled.h"
#include "shared_queue.h"
#include "log_msg_tag.h"

namespace xlog
{
namespace detail
{

// N.B. The notify of the condition_variable is moved intentionally into
// the lock scope. The reasons are the following:
// 1. There are two possible pthreads implementations. In the first one
// the thread waiting on a condition variable will not be wakeup immediately
// when the condition variable is notified, but instead it'll be put in
// the mutex queue for wakeup when the mutex gets unlocked. In the second
// implementation it's more effective to notify the condition variable outside
// the mutex lock because the waiting thread may get waked-up just to be
// put again on sleep in order to wait for the mutex to get unlocked. However,
// a lot of people claim that you can't rely on particular pthread
// implementation and this whole thing is a micro-optimization without
// visible effect.
// 2. Moving the condition_variable notify inside the mutex scope supresses
// valgrind (helgrind) warnings about this (although they are false positive).

shared_queue::shared_queue(uint32_t max_size) noexcept
    : max_allowed_size_(max_size)
{
}

shared_queue::~shared_queue() noexcept
{
}

void shared_queue::emplace(time_t timestamp, const char* data, uint32_t size,
                           target_id tid, level lvl, bool force) noexcept
{
    using xutils::tagged_buffer;
    // Create the message outside the lock
    auto m = tagged_buffer<log_msg_tag>::create(size, timestamp, data, size,
                                                tid, lvl);
    {
        lock_guard_t _(mutex_);
        if ((!block_push_ && (queue_.size() <= max_allowed_size_)) || force)
        {
            queue_.push(std::move(m));
            cond_var_.notify_one();
        }
        else
        {
            ++cnt_blocked_push_;
            block_push_ = true;
        }
    }
}

shared_queue::pop_result shared_queue::wait_pop(msg_type& v) noexcept
{
    pop_result res;

    std::unique_lock<std::mutex> lk(mutex_);
    cond_var_.wait(lk, [this]
                   {
                       return !queue_.empty() || unblock_pop_;
                   });
    unblock_pop_ = false;
    const auto s = queue_.size();
    if (s > 0)
    {
        v = queue_.pop();

        // We intentionally return the size before pop in order to
        // indicate that the pop succeeds.
        res.queue_size_   = s;
        res.push_blocked_ = block_push_;
    }

    return res;
}

shared_queue::pop_result shared_queue::try_pop(msg_type& v) noexcept
{
    pop_result res;

    lock_guard_t _(mutex_);
    const auto s = queue_.size();
    if (s > 0)
    {
        v = queue_.pop();

        // We intentionally return the size before pop in order to
        // indicate that the pop succeeds.
        res.queue_size_   = s;
        res.push_blocked_ = block_push_;
    }

    return res;
}

void shared_queue::block_push() noexcept
{
    lock_guard_t _(mutex_);
    block_push_ = true;
}

void shared_queue::unblock_push() noexcept
{
    lock_guard_t _(mutex_);
    cnt_blocked_push_ = 0;
    block_push_       = false;
}

void shared_queue::unblock_pop() noexcept
{
    lock_guard_t _(mutex_);
    unblock_pop_ = true;
    cond_var_.notify_one();
}

void shared_queue::unblock_pop_if_empty() noexcept
{
    lock_guard_t _(mutex_);
    if (queue_.empty())
    {
        unblock_pop_ = true;
        cond_var_.notify_one();
    }
}

uint32_t shared_queue::count_blocked_push() const noexcept
{
    lock_guard_t _(mutex_);
    return cnt_blocked_push_;
}

} // namespace detail
} // namespace xlog
