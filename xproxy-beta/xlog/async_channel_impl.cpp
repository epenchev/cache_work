#include "precompiled.h"
#include "async_channel_impl.h"
#include "log_target_impl.h"
#include "log_msg_tag.h"
#include "xlog_common.h"

namespace xlog
{
namespace detail
{

async_channel_impl::async_channel_impl(const string_view_t& name,
                                       uint32_t hard_lim,
                                       uint32_t soft_lim) noexcept
    : queue_(hard_lim),
      soft_lim_(soft_lim),
      name_(name.data(), name.size())
{
    X3ME_ENFORCE(hard_lim > soft_lim);
    X3ME_ENFORCE(soft_lim > 1);
}

async_channel_impl::~async_channel_impl() noexcept
{
    X3ME_ENFORCE(!worker_.joinable()); // stop method must have been called
}

bool async_channel_impl::add_log_target(target_id tid,
                                        target_ptr_t&& t) noexcept
{
    assert(stopped_);
    bool res     = false;
    auto cmp_tgt = [tid](const target_info& i)
    {
        return tid.value() == i.tid_.value();
    };
    const auto tg_log_level = t->max_log_level();
    if ((targets_.size() < max_cnt_targets) &&
        (std::find_if(targets_.cbegin(), targets_.cend(), cmp_tgt) ==
         targets_.cend()))
    {
        targets_.push_back(target_info{std::move(t), tid});
        res = true;
        if (tg_log_level > max_log_level_)
            max_log_level_ = tg_log_level;
    }
    return res;
}

bool async_channel_impl::add_explicit_log_target(target_id tid,
                                                 target_ptr_t&& t) noexcept
{
    assert(stopped_);
    bool res     = false;
    auto cmp_tgt = [tid](const target_info& i)
    {
        return tid.value() == i.tid_.value();
    };
    const auto tg_log_level = t->max_log_level();
    if ((expl_targets_.size() < max_cnt_expl_targets) &&
        (std::find_if(expl_targets_.cbegin(), expl_targets_.cend(), cmp_tgt) ==
         expl_targets_.cend()))
    {
        expl_targets_.push_back(target_info{std::move(t), tid});
        res = true;
        if (tg_log_level > max_log_level_expl_)
            max_log_level_expl_ = tg_log_level;
    }
    return res;
}

void async_channel_impl::start() noexcept
{
    assert(stopped_);
    stopped_ = flag_running;
    worker_  = std::thread([this]
                          {
                              worker();
                          });
}

void async_channel_impl::stop(bool wait_flush) noexcept
{
    assert(stopped_ == flag_running);
    stopped_ = wait_flush ? flag_stopped_flush : flag_stopped;
    queue_.block_push();
    if (wait_flush)
        queue_.unblock_pop_if_empty(); // Wait if the queue is non-empty
    else
        queue_.unblock_pop(); // Don't wait, unblock unconditionally
    if (worker_.joinable())
        worker_.join();
}

////////////////////////////////////////////////////////////////////////////////

void async_channel_impl::worker() noexcept
{
    x3me::sys_utils::set_this_thread_name(name_.c_str());

    uint8_t stopped = flag_running;
    // N.B. Helgrind gives warnings for this code and the stop code above.
    // However, I truly believe that they are false-positive. It just that
    // the helgrind is not very good to see happens-before relationships
    // for non POSIX thread primitives and thus it gives warnings for
    // atomic operations which uses the gcc atomic builtins.
    // The passed memory order doesn't change the warnings. Tried with
    // sequentially consistent memory order but it was the same.
    while ((stopped = stopped_.load(std::memory_order_acquire)) == flag_running)
    {
        shared_queue::msg_type msg;
        const auto res = queue_.wait_pop(msg);
        if (res)
        {
            log_message(msg);
            if (res.push_blocked_ && (res.queue_size_ < soft_lim_))
            {
                // This count is not very precise because there may
                // appear few more blocked messages before the call to
                // unblock push below. However, the exact count is not so
                // important in my opinion as the correct position of the
                // report message. If we unblock the queue here, the message
                // may get logged after few of the unblocked messages.
                const uint32_t cnt_skipped = queue_.count_blocked_push();
                // The blocked count may be 0 when we unblock the queue to exit.
                if (cnt_skipped > 0)
                {
                    char buf[128];
                    const int len =
                        snprintf(buf, sizeof(buf),
                                 "%s. Logger queue limit has been reached. Now "
                                 "it's ok. %u log messages has been skipped\n",
                                 level_str(level::error), cnt_skipped);
                    X3ME_ENFORCE(len > 0);
                    // If the channel gets stopped before this
                    // message is actually logged and the flush is not requested
                    // then nobody will see the message. It's OK (IMO).
                    queue_.emplace(time(nullptr), buf, len, invalid_target_id,
                                   level::error, true /*force*/);
                }
                queue_.unblock_push();
            }
        }
    }
    // The queue push gets blocked on stop, but it may get unblocked after
    // the fact from the above code. The above loop will exit but the queue
    // will be unblocked. Just to be sure we need to block it again here.
    queue_.block_push();
    if (stopped == flag_stopped_flush)
    { // We need to flush all pending log messages
        // N.B. The external/logger system ensures that nobody is queuing
        // messages to the this channel.
        shared_queue::msg_type msg;
        while (queue_.try_pop(msg))
        {
            log_message(msg);
        }
    }
    // Lastly log if some log messages has been skipped.
    if (const uint32_t cnt_skipped = queue_.count_blocked_push())
    {
        char buf[128];
        const int len =
            snprintf(buf, sizeof(buf),
                     "%s. Logger queue limit has been reached. %u log messages "
                     "has been skipped\n",
                     level_str(level::error), cnt_skipped);
        X3ME_ENFORCE(len > 0);
        log_message(time(nullptr), buf, len, invalid_target_id,
                    to_number(level::error));
    }
}

void async_channel_impl::log_message(const shared_queue::msg_type& msg) noexcept
{
    log_message(msg->timestamp(), msg->data(), msg->size(),
                msg->get_target_id(), msg->get_level());
}

void async_channel_impl::log_message(time_t timestamp, const char* data,
                                     uint32_t size, target_id tid,
                                     level_type lvl) noexcept
{
    // N.B. Valgrind (helgrind) gives warning about this function.
    // It's described as thread safe of its documentation, but this is not
    // entirely true. According to valgrind this function reads internally
    // the timezone information (environment variable TZ)
    // which may be set by some other thread
    // via tzset or something like that. However, I don't want to search
    // for special date time library (the boost one is awful).
    // I don't think we'll ever going to set the time zone information in
    // our application and thus let it be in this way.
    // In addition, this call is specifically moved to the logging thread
    // because it take almost 1us out of the 2us needed for format and
    // en-queuing of every log message.
    tm ctm;
    localtime_r(&timestamp, &ctm);

    size_t len = 0;
    char buf[32];
    try
    {
        fmt::ArrayWriter aw(buf);
        aw.write("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} | ",
                 1900 + ctm.tm_year, ctm.tm_mon + 1, ctm.tm_mday, ctm.tm_hour,
                 ctm.tm_min, ctm.tm_sec);
        len = aw.size();
    }
    catch (...)
    {
        const char msg[] = "Date-time error | ";
        strcpy(buf, msg);
        len = sizeof(msg) - 1;
    }

    hdr_data_t hd;
    hd[0].iov_base = buf;
    hd[0].iov_len  = len;
    hd[1].iov_base = const_cast<char*>(data); // Yeah, that's sad
    hd[1].iov_len  = size;

    if (tid.value() == invalid_target_id.value())
    { // It's a message to non explicit log targets
        bool logged = false;
        // A possible optimization here is to store the max_log_level
        // of the target into the target info structure. Thus we can avoid
        // one virtual call here. However, for the two virtual calls the
        // compiler will fetch the virtual pointer only once and thus we
        // won't save so much.
        for (auto& t : targets_)
        {
            if (t.tgt_->max_log_level() >= lvl)
            {
                logged = true;
                t.tgt_->write(hd);
            }
        }
// It would be bug in the logger logic if it sends message through
// the async channel which no target wants to log
#ifndef X3ME_TEST
        X3ME_ENFORCE(logged);
#else
        (void)logged;
#endif // X3ME_TEST
    }
    else
    {
        // This is a message to a specific target which must be present.
        // Otherwise it'd be a bug.
        auto it = std::find_if(expl_targets_.begin(), expl_targets_.end(),
                               [tid](const target_info& i)
                               {
                                   return i.tid_.value() == tid.value();
                               });
// The logger must not enqueue messages which won't be logged
#ifndef X3ME_TEST
        X3ME_ENFORCE(it != expl_targets_.end());
        X3ME_ENFORCE(it->tgt_->max_log_level() >= lvl);
#else
        if ((it != expl_targets_.end()) && (it->tgt_->max_log_level() >= lvl))
#endif // X3ME_TEST
        {
            it->tgt_->write(hd);
        }
    }
}

} // namespace detail
} // namespace xlog
