#pragma once

namespace xutils
{

/// Thread safe queue.
/// The queue is marked as noexcept although the mutex lock may throw exception.
/// However, IMO is better to crash the application in this case than to
/// complicate the application code with needless (IMO) logic. I'm yet to see
/// std::mutex to throw exception on lock().
template <typename T>
class shared_queue
{
    using queue_t      = std::queue<T>;
    using lock_guard_t = std::lock_guard<std::mutex>;

    queue_t queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    bool unblock_ = false;

public:
    using value_type = typename queue_t::value_type;
    using size_type  = typename queue_t::size_type;

public:
    shared_queue() noexcept {}
    ~shared_queue() noexcept {}

    shared_queue& operator=(const shared_queue&) = delete;
    shared_queue(const shared_queue&) = delete;
    shared_queue& operator=(shared_queue&&) = delete;
    shared_queue(shared_queue&&) = delete;

    /// Pushes item copying it onto the queue.
    void push(const value_type& v) noexcept
    {
        {
            lock_guard_t _(mutex_);
            queue_.push(v);
        }
        cond_var_.notify_one();
    }

    /// Pushes item moving it onto the queue.
    void push(value_type&& v) noexcept
    {
        {
            lock_guard_t _(mutex_);
            queue_.push(std::move(v));
        }
        cond_var_.notify_one();
    }

    /// Pushes new item constructing it in-place using the passed arguments
    template <typename... Args>
    void emplace(Args&&... args) noexcept
    {
        {
            lock_guard_t _(mutex_);
            queue_.emplace(std::forward<Args>(args)...);
        }
        cond_var_.notify_one();
    }

    /// Tries to pop an item from the queue. If the queue is empty at the
    /// moment it blocks waiting for an item or until the queue gets
    /// explicitly unblocked (see unblock).
    /// Returns true if an item is successfully popped from the queue.
    /// Returns false if no item has been popped from the queue i.e. the
    /// queue has been explicitly unblocked.
    bool wait_pop(value_type& v) noexcept
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cond_var_.wait(lk, [this]
                       {
                           return !queue_.empty() || unblock_;
                       });
        unblock_ = false;
        if (!queue_.empty())
        {
            v = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }

    /// Returns true if the queue is not empty and an item is actually popped.
    /// Returns false otherwise.
    bool try_pop(value_type& v) noexcept
    {
        lock_guard_t _(mutex_);
        if (!queue_.empty())
        {
            v = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }

    /// Unblocks the current waiting on the queue. If there is no
    /// current waiting operation it unblocks the next waiting operation.
    void unblock() noexcept
    {
        lock_guard_t _(mutex_);
        unblock_ = true;
    }

    /// Returns true if the queue is empty and false otherwise.
    bool empty() const noexcept
    {
        lock_guard_t _(mutex_);
        return queue_.empty();
    }

    /// Returns the current size of the queue. Not really useful
    /// because the size may change between two calls.
    size_type size() const noexcept
    {
        lock_guard_t _(mutex_);
        return queue_.size();
    }
};

} // namespace xutils
