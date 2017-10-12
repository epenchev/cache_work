#include "async_channel.h"
#include "async_channel_impl.h"

namespace xlog
{

template <typename Tag>
logger<Tag>::logger(private_tag) noexcept
{
    // TODO Replace these with is_always_lock_free when available.
    X3ME_ENFORCE(max_log_level_.is_lock_free());
    X3ME_ENFORCE(max_log_level_expl_.is_lock_free());
}

template <typename Tag>
logger<Tag>::~logger() noexcept
{
    // The async_channels needs to be explicitly stopped before their
    // destruction.
    for (auto& ch : channels_)
        ch.chan_->stop(true /*Wait flush*/);
}

template <typename Tag>
bool logger<Tag>::add_async_channel(const channel_id& id, async_channel&& ch,
                                    const filter_fn_t& filter_fn) noexcept
{
    bool res      = false;
    auto cmp_chan = [id](const chan_info& i)
    {
        return id.value() == i.chan_id_.value();
    };
    const auto ch_log_level      = ch.impl_->max_log_level();
    const auto ch_log_level_expl = ch.impl_->max_log_level_expl();
    unique_lock_t _(op_mutex_);
    if ((channels_.size() < max_cnt_channels) &&
        (std::find_if(channels_.cbegin(), channels_.cend(), cmp_chan) ==
         channels_.cend()))
    {
        ch.impl_->start();
        channels_.push_back(chan_info{std::move(ch.impl_), filter_fn, id});
        res = true;
        // We need release semantics of the log levels because they are
        // checked without locking in the write log case.
        // The log levels still needs to be inside the mutex because channels
        // could be added/removed from more than 1 thread simultaneously.
        if (ch_log_level > max_log_level_)
        {
            max_log_level_.store(ch_log_level, std::memory_order_release);
        }
        if (ch_log_level_expl > max_log_level_expl_)
        {
            max_log_level_expl_.store(ch_log_level_expl,
                                      std::memory_order_release);
        }
    }
    return res;
}

template <typename Tag>
bool logger<Tag>::rem_async_channel(const channel_id& id,
                                    bool wait_flush) noexcept
{
    bool res      = false;
    auto cmp_chan = [id](const chan_info& i)
    {
        return id.value() == i.chan_id_.value();
    };
    auto max_log_level = [](const channels_t& chns)
    {
        auto it = std::max_element(
            chns.cbegin(), chns.cend(),
            [](const chan_info& lhs, const chan_info& rhs)
            {
                return lhs.chan_->max_log_level() < rhs.chan_->max_log_level();
            });
        // We won't find anything in case of no channels
        return (it != chns.cend()) ? it->chan_->max_log_level()
                                   : to_number(level::off);

    };
    auto max_log_level_expl = [](const channels_t& chns)
    {
        auto it =
            std::max_element(chns.cbegin(), chns.cend(),
                             [](const chan_info& lhs, const chan_info& rhs)
                             {
                                 return lhs.chan_->max_log_level_expl() <
                                        rhs.chan_->max_log_level_expl();
                             });
        // We won't find anything in case of no channels
        return (it != chns.cend()) ? it->chan_->max_log_level_expl()
                                   : to_number(level::off);

    };

    // Destroy the channel outside the mutex
    channel_ptr_t chan;
    {
        unique_lock_t _(op_mutex_);
        auto it = std::find_if(channels_.begin(), channels_.end(), cmp_chan);
        if (it != channels_.end())
        {
            chan = std::move(it->chan_);
            channels_.erase(it);
            res = true;

            assert(chan->max_log_level() <= max_log_level_);
            assert(chan->max_log_level_expl() <= max_log_level_expl_);

            const auto max_ll      = max_log_level(channels_);
            const auto max_ll_expl = max_log_level_expl(channels_);
            // We need release semantics of the log levels because they are
            // checked without locking in the write log case.
            // The log levels still needs to be inside the mutex because
            // channels could be added/removed from more than 1 thread.
            max_log_level_.store(max_ll, std::memory_order_release);
            max_log_level_expl_.store(max_ll_expl, std::memory_order_release);
        }
    }
    chan->stop(wait_flush);
    return res;
}

template <typename Tag>
void logger<Tag>::write_impl(level lvl, const Tag& tag, const char* data,
                             uint32_t size) noexcept
{
    // We may use gettimeofday if we need ms/us in the log
    const auto ct = time(nullptr);
    // Always enqueue the log messages with fatal level if the queue limit
    // has been reached.
    const bool force = (lvl == level::fatal);
    shared_lock_t _(op_mutex_);
    for (auto& ch : channels_)
    {
        // We'll enqueue the message either by it's log level
        // or if the filter function is set and it allows it.
        if ((to_number(lvl) <= ch.chan_->max_log_level()) ||
            (ch.filter_fn_ && ch.filter_fn_(lvl, tag)))
        {
            ch.chan_->enque_log_msg(ct, lvl, invalid_target_id, data, size,
                                    force);
        }
    }
}

template <typename Tag>
void logger<Tag>::write_impl(level lvl, channel_id cid, target_id tid,
                             const Tag& tag, const char* data,
                             uint32_t size) noexcept
{
    // We may use gettimeofday if we need ms/us in the log
    const auto ct = time(nullptr);

    shared_lock_t _(op_mutex_);
    // Search for the channel first
    uint32_t chpos = channels_.size();
    // We'll most likely choose consecutive id numbers.
    // Let's try fast case first.
    if ((cid.value() < channels_.size()) &&
        (cid.value() == channels_[cid.value()].chan_id_.value()))
    {
        chpos = cid.value();
    }
    else // The "slow" case. Not so slow of course because there are 1-2-3.
    {
        auto it = std::find_if(channels_.cbegin(), channels_.cend(),
                               [cid](const chan_info& i)
                               {
                                   return i.chan_id_.value() == cid.value();
                               });
        // The channel must be found. A precondition to this function is
        // for the caller to pass valid channel and target ids.
        X3ME_ENFORCE(it != channels_.cend());
        chpos = it - channels_.cbegin();
    }
    // Enqueue the log message on the found channel
    auto& ch = channels_[chpos];
    if ((to_number(lvl) <= ch.chan_->max_log_level_expl()) ||
        (ch.filter_fn_ && ch.filter_fn_(lvl, tag)))
    {
        // We want to forcefully queue the explicit log messages even
        // if the queue limit has been reached.
        ch.chan_->enque_log_msg(ct, lvl, tid, data, size, true /*forced*/);
    }
}

template <typename Tag>
void logger<Tag>::add_header(fmt::MemoryWriter& mw, level lvl, const Tag& tag,
                             const char* file_line)
{
    // Avoid potential calls to the OS
    static const auto pid              = x3me::sys_utils::process_id();
    static const thread_local auto tid = x3me::sys_utils::thread_id();
    if (X3ME_UNLIKELY(file_line))
    {
        // PID | TID | Level | Tag | file_line
        mw.write("{:d} | {:d} | {:>5} | {} | {} | ", pid, tid, lvl, tag,
                 file_line);
    }
    else
    {
        // PID | TID | Level | Tag
        mw.write("{:d} | {:d} | {:>5} | {} | ", pid, tid, lvl, tag);
    }
}

} // namespace xlog
