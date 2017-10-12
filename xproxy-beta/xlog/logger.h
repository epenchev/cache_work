#pragma once

#include "xlog_common.h"

namespace xlog
{
class async_channel;
template <typename Tag>
class logger;
template <typename Tag>
using logger_ptr = std::unique_ptr<logger<Tag>>;

namespace detail
{
class async_channel_impl;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

template <typename Tag>
class logger
{
    static constexpr uint16_t max_cnt_channels = 4;

    using channel_ptr_t = std::unique_ptr<detail::async_channel_impl>;
    using filter_fn_t   = std::function<bool(level, const Tag&)>;
    struct chan_info
    {
        channel_ptr_t chan_;
        filter_fn_t filter_fn_;
        channel_id chan_id_;
    };
    using channels_t =
        boost::container::static_vector<chan_info, max_cnt_channels>;
    using operations_mutex_t = x3me::thread::shared_mutex;
    using unique_lock_t      = std::lock_guard<operations_mutex_t>;
    using shared_lock_t      = x3me::thread::shared_lock;
    using atomic_level_t     = std::atomic<level_type>;

    channels_t channels_;
    operations_mutex_t op_mutex_;

    atomic_level_t max_log_level_{to_number(level::off)};
    atomic_level_t max_log_level_expl_{to_number(level::off)};

    template <typename T>
    friend logger_ptr<T> create_logger() noexcept;
    struct private_tag
    {
    };

public:
    explicit logger(private_tag) noexcept;
    ~logger() noexcept;

    logger(const logger&) = delete;
    logger& operator=(const logger&) = delete;
    logger(logger&&) = delete;
    logger& operator=(logger&&) = delete;

    /// Returns true if the channel is successfully added.
    /// Returns false if the max count of channels is reached and
    /// the channel can't be added.
    bool
    add_async_channel(const channel_id& id, async_channel&& ch,
                      const filter_fn_t& filter_fn = filter_fn_t()) noexcept;
    /// The wait_flush parameter determines if the call will wait for
    /// the channel until it flushes all or will remove the channel immediately.
    /// Returns true if the channel is found and removed.
    /// Returns false if there is no channel with such id.
    bool rem_async_channel(const channel_id& id, bool wait_flush) noexcept;

    template <typename... Args>
    void write(level lvl, const Tag& tag, fmt::CStringRef format,
               const Args&... args) noexcept;

    /// writes log explicitly choosing given channel and target. no other
    /// channels and targets will be checked
    template <typename... Args>
    void write(level lvl, channel_id cid, target_id tid, const Tag& tag,
               fmt::CStringRef format, const Args&... args) noexcept;

    template <typename... Args>
    void write_fl(level lvl, const Tag& tag, const char* file_line,
                  fmt::CStringRef format, const Args&... args) noexcept;
    /// writes log explicitly choosing given channel and target. no other
    /// channels and targets will be checked
    template <typename... Args>
    void write_fl(level lvl, channel_id cid, target_id tid, const Tag& tag,
                  const char* file_line, fmt::CStringRef format,
                  const Args&... args) noexcept;

private:
    void write_impl(level lvl, const Tag& tag, const char* data,
                    uint32_t size) noexcept;
    void write_impl(level lvl, channel_id cid, target_id tid, const Tag& tag,
                    const char* data, uint32_t size) noexcept;
    static void add_header(fmt::MemoryWriter& mw, level lvl, const Tag& tag,
                           const char* file_line);
};

////////////////////////////////////////////////////////////////////////////////

template <typename Tag>
logger_ptr<Tag> create_logger() noexcept
{
    return std::make_unique<logger<Tag>>(typename logger<Tag>::private_tag{});
}

////////////////////////////////////////////////////////////////////////////////

template <typename Tag>
template <typename... Args>
void logger<Tag>::write(level lvl, const Tag& tag, fmt::CStringRef format,
                        const Args&... args) noexcept
{
    write_fl(lvl, tag, nullptr, format, args...);
}

template <typename Tag>
template <typename... Args>
void logger<Tag>::write(level lvl, channel_id cid, target_id tid,
                        const Tag& tag, fmt::CStringRef format,
                        const Args&... args) noexcept
{
    write_fl(lvl, cid, tid, tag, nullptr, format, args...);
}

template <typename Tag>
template <typename... Args>
void logger<Tag>::write_fl(level lvl, const Tag& tag, const char* file_line,
                           fmt::CStringRef format, const Args&... args) noexcept
{
    // N.B. Even if the max_log_level changes while we format the message
    // in this thread the write_impl won't enqueue the message if the
    // channel max_log_level doesn't allow it.
    if (to_number(lvl) <= max_log_level_.load(std::memory_order_acquire))
    {
        // The message log level is lesser than the stored max_log_level.
        // This means that there is at least one channel and target which
        // needs to receive this message.
        fmt::MemoryWriter mw;
        try
        {
            add_header(mw, lvl, tag, file_line);
            mw.write(format, args...);
            mw << '\n';
        }
        catch (const std::exception& ex)
        { // Really don't know what else to do in this case.
            // This is a bug and the passed format needs to be fixed.
            std::cerr << "Wrong log message format. " << format.c_str() << ". "
                      << ex.what() << std::endl;
            assert(false);
            return;
        }

        write_impl(lvl, tag, mw.data(), mw.size());
    }
}

template <typename Tag>
template <typename... Args>
void logger<Tag>::write_fl(level lvl, channel_id cid, target_id tid,
                           const Tag& tag, const char* file_line,
                           fmt::CStringRef format, const Args&... args) noexcept
{
    // N.B. Even if the max_log_level_expl changes while we format the message
    // in this thread the write_impl won't enqueue the message if the
    // channel max_log_level_expl doesn't allow it.
    if (to_number(lvl) <= max_log_level_expl_.load(std::memory_order_acquire))
    {
        fmt::MemoryWriter mw;
        try
        {
            add_header(mw, lvl, tag, file_line);
            mw.write(format, args...);
            mw << '\n';
        }
        catch (const std::exception& ex)
        { // Really don't know what else to do in this case.
            // This is a bug and the passed format needs to be fixed.
            std::cerr << "Wrong log message format. " << format.c_str() << ". "
                      << ex.what() << std::endl;
            assert(false);
            return;
        }

        write_impl(lvl, cid, tid, tag, mw.data(), mw.size());
    }
}

} // namespace xlog
