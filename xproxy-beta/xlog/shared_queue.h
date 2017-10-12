#pragma once

#include "xutils/tagged_buffer_queue.h"
#include "log_msg_tag.h"

namespace xlog
{
namespace detail
{

class shared_queue
{
    using queue_t      = xutils::tagged_buffer_queue<log_msg_tag>;
    using lock_guard_t = std::lock_guard<std::mutex>;

    queue_t queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    uint32_t cnt_blocked_push_ = 0;
    const uint32_t max_allowed_size_;
    bool block_push_  = false;
    bool unblock_pop_ = false;

public:
    using msg_type = xutils::tagged_buffer_ptr_t<log_msg_tag>;

    struct pop_result
    {
        uint32_t queue_size_ = 0;
        bool push_blocked_   = false;

        explicit operator bool() const noexcept { return queue_size_ != 0; }
    };

public:
    explicit shared_queue(uint32_t max_size) noexcept;
    ~shared_queue() noexcept;

    shared_queue() = delete;
    shared_queue& operator=(const shared_queue&) = delete;
    shared_queue(const shared_queue&) = delete;
    shared_queue& operator=(shared_queue&&) = delete;
    shared_queue(shared_queue&&) = delete;

    void emplace(time_t timestamp, const char* data, uint32_t size,
                 target_id tid, level lvl, bool force) noexcept;

    pop_result wait_pop(msg_type& v) noexcept;
    pop_result try_pop(msg_type& v) noexcept;

    void block_push() noexcept;
    void unblock_push() noexcept;

    void unblock_pop() noexcept;
    void unblock_pop_if_empty() noexcept;

    uint32_t count_blocked_push() const noexcept;
};

} // namespace detail
} // namespace xlog
