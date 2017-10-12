#pragma once

#include "aio_task_queue.h"

namespace cache
{
namespace detail
{

class aio_task;
class volume_fd;

class aio_service
{
    // Don't go to the heap for the most common case.
    // Waste some memory if the threads are more.
    // However, once we find the 'good' number we won't change it.
    using threads_t = boost::container::small_vector<std::thread, 8>;

    volume_fd& vol_fd_;
    threads_t threads_;
    aio_task_queue read_queue_;
    aio_task_queue write_queue_;

public:
    // We need at least one thread for writing and one for reading.
    // So that the write operations don't block the reads.
    // However it may happen that the both threads are occupied by reads
    // at a given moment.
    static constexpr uint16_t min_num_threads = 2;

public:
    explicit aio_service(volume_fd& vol_fd) noexcept;
    ~aio_service() noexcept;

    aio_service(const aio_service&) = delete;
    aio_service& operator=(const aio_service&) = delete;
    aio_service(aio_service&&) = delete;
    aio_service& operator=(aio_service&&) = delete;

    void start(const boost::container::string& vol_path,
               uint16_t num_threads) noexcept;
    void stop();

    uint32_t read_queue_size() const noexcept { return read_queue_.size(); }
    uint32_t write_queue_size() const noexcept { return write_queue_.size(); }

    // The aio_service starts to share the ownership of the task,
    // when the latter gets pushed to one of the queues.
    void push_front_read_queue(owner_ptr_t<aio_task> t) noexcept
    {
        push_front_task(t, read_queue_);
    }
    void push_read_queue(owner_ptr_t<aio_task> t) noexcept
    {
        push_task(t, read_queue_);
    }
    void enqueue_read_queue(owner_ptr_t<aio_task> t) noexcept
    {
        enqueue_task(t, read_queue_);
    }

    // There are cases when we need to push read tasks to the write queue,
    // for example in the evacuation case. Posting this to the read queue
    // would complicate the code, because we don't want to proceed any
    // more write tasks before the evacuation is done.
    void push_front_write_queue(owner_ptr_t<aio_task> t) noexcept
    {
        push_front_task(t, write_queue_);
    }
    void push_write_queue(owner_ptr_t<aio_task> t) noexcept
    {
        push_task(t, write_queue_);
    }
    void enqueue_write_queue(owner_ptr_t<aio_task> t) noexcept
    {
        enqueue_task(t, write_queue_);
    }

    bool cancel_task_read_queue(non_owner_ptr_t<aio_task> t) noexcept
    {
        return cancel_task(t, read_queue_);
    }
    bool cancel_task_write_queue(non_owner_ptr_t<aio_task> t) noexcept
    {
        return cancel_task(t, write_queue_);
    }

private:
    static void process_queue(aio_task_queue& queue, volume_fd& fd) noexcept;
    static void push_front_task(owner_ptr_t<aio_task> t,
                                aio_task_queue& queue) noexcept;
    static void push_task(owner_ptr_t<aio_task> t,
                          aio_task_queue& queue) noexcept;
    static void enqueue_task(owner_ptr_t<aio_task> t,
                             aio_task_queue& queue) noexcept;
    static bool cancel_task(non_owner_ptr_t<aio_task> t,
                            aio_task_queue& queue) noexcept;
    static void clear_queue_on_stop(aio_task_queue& queue) noexcept;
};

} // namespace detail
} // namespace cache
