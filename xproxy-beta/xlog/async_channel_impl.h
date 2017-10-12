#pragma once

#include "xlog_common.h"
#include "shared_queue.h"

namespace xlog
{
namespace detail
{
struct log_target_impl;

// The async channel public interface is not thread safe.
// It's internal functionality which is synchronized by the logger itself.
class async_channel_impl
{
public:
    // The channel name is set as a name of the corresponding logging thread.
    // The OS API pthread_setname_np limits the thread name to 15.
    static constexpr uint16_t max_len_channel_name = 15;
    static constexpr uint16_t max_cnt_targets      = 4;
    static constexpr uint16_t max_cnt_expl_targets = 2;

private:
    enum : uint8_t
    {
        flag_running,
        flag_stopped,
        flag_stopped_flush,
    };

    using name_t       = stack_string_t<max_len_channel_name + 1>;
    using target_ptr_t = std::unique_ptr<log_target_impl>;
    struct target_info
    {
        target_ptr_t tgt_;
        target_id tid_;
    };
    using targets_t =
        boost::container::static_vector<target_info, max_cnt_targets>;
    using explicit_targets_t =
        boost::container::static_vector<target_info, max_cnt_expl_targets>;

    shared_queue queue_;
    std::thread worker_;
    targets_t targets_;
    explicit_targets_t expl_targets_;
    level_type max_log_level_      = to_number(level::off);
    level_type max_log_level_expl_ = to_number(level::off);
    const uint32_t soft_lim_;
    std::atomic_uchar stopped_{flag_stopped};
    const name_t name_;

public:
    async_channel_impl(const string_view_t& name, uint32_t hard_lim,
                       uint32_t soft_lim) noexcept;
    ~async_channel_impl() noexcept;

    async_channel_impl() = delete;
    async_channel_impl(const async_channel_impl&) = delete;
    async_channel_impl& operator=(const async_channel_impl&) = delete;
    async_channel_impl(async_channel_impl&&) = delete;
    async_channel_impl& operator=(async_channel_impl&&) = delete;

    bool add_log_target(target_id tid, target_ptr_t&& t) noexcept;
    bool add_explicit_log_target(target_id tid, target_ptr_t&& t) noexcept;

    void start() noexcept;
    void stop(bool wait_flush) noexcept;

    // This is the only method which is safe to be called from multiple threads
    void enque_log_msg(time_t timestamp, level lvl, target_id tid,
                       const char* data, uint32_t size, bool force) noexcept
    {
        queue_.emplace(timestamp, data, size, tid, lvl, force);
    }

    level_type max_log_level() const noexcept { return max_log_level_; }
    level_type max_log_level_expl() const noexcept
    {
        return max_log_level_expl_;
    }

private:
    void worker() noexcept;
    void log_message(const shared_queue::msg_type& msg) noexcept;
    void log_message(time_t timestamp, const char* data, uint32_t size,
                     target_id tid, level_type lvl) noexcept;
};

} // namespace detail
} // namespace xlog
