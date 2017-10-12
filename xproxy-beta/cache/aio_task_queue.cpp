#include "precompiled.h"
#include "aio_task_queue.h"

namespace cache
{
namespace detail
{

aio_task_queue::aio_task_queue() noexcept
{
}

aio_task_queue::~aio_task_queue() noexcept
{
    X3ME_ASSERT(
        queue_.empty(),
        "The tasks must have been released, otherwise we are leaking them");
}

bool aio_task_queue::push_front(owner_ptr_t<aio_task> t) noexcept
{
    lock_guard_t _(mutex_);
    if (X3ME_LIKELY(working_))
    {
        queue_.push_front(*t);
        // I believe 'synchronize with' semantic is not needed between all
        // of the store operations because they happen inside the mutex
        // critical section. Thus we don't need to use memory_order_acq_rel.
        size_.fetch_add(1, std::memory_order_release);
        // It's a good rule of thumb to notify outside the held lock.
        // However, I don't notify the condition variable outside the lock,
        // because the pthreads implementation with the wait morphing
        // encourages the current way.
        // In addition the helgrind complains if the condition_variable
        // is notified outside the lock.
        cond_var_.notify_one();
        return true;
    }
    return false;
}

bool aio_task_queue::push_back(owner_ptr_t<aio_task> t) noexcept
{
    lock_guard_t _(mutex_);
    if (X3ME_LIKELY(working_))
    {
        queue_.push_back(*t);
        size_.fetch_add(1, std::memory_order_release);
        cond_var_.notify_one();
        return true;
    }
    return false;
}

aio_task_queue::enqueue_res
aio_task_queue::enqueue(owner_ptr_t<aio_task> t) noexcept
{
    lock_guard_t _(mutex_);
    if (X3ME_LIKELY(working_))
    {
        if (!t->is_linked())
        {
            queue_.push_back(*t);
            size_.fetch_add(1, std::memory_order_release);
            cond_var_.notify_one();
            return enqueue_res::enqueued;
        }
        return enqueue_res::skipped;
    }
    return enqueue_res::stopped;
}

owner_ptr_t<aio_task> aio_task_queue::pop() noexcept
{
    // Returns null task if the queue is explicitly unblocked.
    owner_ptr_t<aio_task> t = nullptr;

    std::unique_lock<std::mutex> lk(mutex_);
    cond_var_.wait(lk, [this]
                   {
                       return !queue_.empty() || !working_;
                   });
    if (working_)
    {
        t = &queue_.front();
        queue_.pop_front();
        size_.fetch_sub(1, std::memory_order_release);
    }

    return t;
}

non_owner_ptr_t<aio_task>
aio_task_queue::remove_task(non_owner_ptr_t<aio_task> t) noexcept
{
    lock_guard_t _(mutex_);
    if (t->is_linked())
    {
        queue_.erase(queue_t::s_iterator_to(*t));
        size_.fetch_sub(1, std::memory_order_release);
    }
    else
    {
        t = nullptr;
    }
    return t;
}

void aio_task_queue::stop() noexcept
{
    lock_guard_t _(mutex_);
    working_ = false;
    // We want to notify all threads waiting on pop.
    cond_var_.notify_all();
}

aio_task_queue::queue_t aio_task_queue::release_all() noexcept
{
    lock_guard_t _(mutex_);
    size_.store(0, std::memory_order_release);
    return std::move(queue_);
}

uint32_t aio_task_queue::size() const noexcept
{
    return size_.load(std::memory_order_acquire);
}

} // namespace detail
} // namespace cache
