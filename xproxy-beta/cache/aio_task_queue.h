#pragma once

#include "aio_task.h"

namespace cache
{
namespace detail
{

class aio_task_queue
{
public:
    using queue_t =
        boost::intrusive::list<aio_task,
                               boost::intrusive::constant_time_size<false>>;

private:
    using lock_guard_t = std::lock_guard<std::mutex>;

    std::mutex mutex_;
    std::condition_variable cond_var_;
    queue_t queue_;
    std::atomic_uint size_{0};
    bool working_ = true;

public:
    aio_task_queue() noexcept;
    ~aio_task_queue() noexcept;

    aio_task_queue(const aio_task_queue&) = delete;
    aio_task_queue& operator=(const aio_task_queue&) = delete;
    aio_task_queue(aio_task_queue&&) = delete;
    aio_task_queue& operator=(aio_task_queue&&) = delete;

    // These two methods expect that the task is not already queued.
    // They return true if the task is put in the queue.
    // They return false if the queue is no longer working and the task is
    // not put in the queue.
    bool push_front(non_owner_ptr_t<aio_task> t) noexcept;
    bool push_back(non_owner_ptr_t<aio_task> t) noexcept;
    // This method enqueues a task to the back if it's not already
    // in the queue, otherwise it's no op and the task remains enqueued
    // at its current position.
    // The method returns true if the task hasn't been in the queue
    // and is enqueued now, returns false otherwise.
    enum struct enqueue_res
    {
        enqueued,
        skipped,
        stopped,
    };
    enqueue_res enqueue(non_owner_ptr_t<aio_task> t) noexcept;

    non_owner_ptr_t<aio_task> pop() noexcept;
    non_owner_ptr_t<aio_task> remove_task(non_owner_ptr_t<aio_task> t) noexcept;

    void stop() noexcept;

    queue_t release_all() noexcept;

    uint32_t size() const noexcept;
};

} // namespace detail
} // namespace cache
