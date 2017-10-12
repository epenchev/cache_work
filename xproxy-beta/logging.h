#pragma once

#include "id_tag.h"
#include "xlog/logger.h"

class settings;

////////////////////////////////////////////////////////////////////////////////

using logger_t     = xlog::logger<id_tag>;
using logger_ptr_t = std::unique_ptr<logger_t>;

extern logger_ptr_t g_log;

constexpr xlog::channel_id main_log_chan(1);
// The debug log channels will start from here up.
constexpr xlog::channel_id debg_log_chan(2);

// The target_ids are unique inside every channel.
constexpr xlog::target_id main_log_id(1);
constexpr xlog::target_id squid_log_id(2);
constexpr xlog::target_id sys_log_id(3);
constexpr xlog::target_id debg_log_id(4);

bool init_logging(const settings& sts) noexcept;

void chown_logs(const std::string& logs_dir, uid_t user_id, gid_t group_id);

/// Returns true in case of valid command and start operation succeed.
/// Returns false in case of error and fills the out_err object with info.
bool start_rt_debug(const settings& sts, string_view_t cmd,
                    std::string& out_err) noexcept;
/// The function returns true if the RT Debug target corresponding to this cmd
/// has been found and removed. It returns false if no such target is found.
/// If the cmd is empty all RT Debug targets are going to be removed.
bool stop_rt_debug(string_view_t cmd, std::string& out_err) noexcept;

////////////////////////////////////////////////////////////////////////////////
// Auxiliary macros which can help if we want to compile out certain log levels

#ifndef X3ME_TEST

#ifndef XLOG_FILE_LINE

#define XLOG_FATAL(tag, format, ...)                                           \
    g_log->write(xlog::level::fatal, tag, format, ##__VA_ARGS__)
#define XLOG_ERROR(tag, format, ...)                                           \
    g_log->write(xlog::level::error, tag, format, ##__VA_ARGS__)
#define XLOG_WARN(tag, format, ...)                                            \
    g_log->write(xlog::level::warn, tag, format, ##__VA_ARGS__)
#define XLOG_INFO(tag, format, ...)                                            \
    g_log->write(xlog::level::info, tag, format, ##__VA_ARGS__)
#define XLOG_DEBUG(tag, format, ...)                                           \
    g_log->write(xlog::level::debug, tag, format, ##__VA_ARGS__)
#define XLOG_TRACE(tag, format, ...)                                           \
    g_log->write(xlog::level::trace, tag, format, ##__VA_ARGS__)

#define XLOG_FATAL_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write(xlog::level::fatal, chan_id, tgt_id, tag, format,             \
                 ##__VA_ARGS__)
#define XLOG_ERROR_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write(xlog::level::error, chan_id, tgt_it, tag, format,             \
                 ##__VA_ARGS__)
#define XLOG_WARN_EXPL(chan_id, tgt_id, tag, format, ...)                      \
    g_log->write(xlog::level::warn, chan_id, tgt_id, tag, format, ##__VA_ARGS__)
#define XLOG_INFO_EXPL(chan_id, tgt_id, tag, format, ...)                      \
    g_log->write(xlog::level::info, chan_id, tgt_id, tag, format, ##__VA_ARGS__)
#define XLOG_DEBUG_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write(xlog::level::debug, chan_id, tgt_id, tag, format,             \
                 ##__VA_ARGS__)
#define XLOG_TRACE_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write(xlog::level::trace, chan_id, tgt_id, tag, format,             \
                 ##__VA_ARGS__)

#else // XLOG_FILE_LINE

#define XLOG_FATAL(tag, format, ...)                                           \
    g_log->write_fl(xlog::level::fatal, tag, __FILE__ ":"__LINE__, format,     \
                    ##__VA_ARGS__)
#define XLOG_ERROR(tag, format, ...)                                           \
    g_log->write_fl(xlog::level::error, tag, __FILE__ ":"__LINE__, format,     \
                    ##__VA_ARGS__)
#define XLOG_WARN(tag, format, ...)                                            \
    g_log->write_fl(xlog::level::warn, tag, __FILE__ ":"__LINE__, format,      \
                    ##__VA_ARGS__)
#define XLOG_INFO(tag, format, ...)                                            \
    g_log->write_fl(xlog::level::info, tag, __FILE__ ":"__LINE__, format,      \
                    ##__VA_ARGS__)
#define XLOG_DEBUG(tag, format, ...)                                           \
    g_log->write_fl(xlog::level::debug, tag, __FILE__ ":"__LINE__, format,     \
                    ##__VA_ARGS__)
#define XLOG_TRACE(tag, format, ...)                                           \
    g_log->write_fl(xlog::level::trace, tag, __FILE__ ":"__LINE__, format,     \
                    ##__VA_ARGS__)

#define XLOG_FATAL_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write_fl(xlog::level::fatal, chan_id, tgt_it, tag,                  \
                    __FILE__ ":"__LINE__, format, ##__VA_ARGS__)
#define XLOG_ERROR_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write_fl(xlog::level::error, chan_id, tgt_it, tag,                  \
                    __FILE__ ":"__LINE__, format, ##__VA_ARGS__)
#define XLOG_WARN_EXPL(chan_id, tgt_id, tag, format, ...)                      \
    g_log->write_fl(xlog::level::warn, chan_id, tgt_it, tag,                   \
                    __FILE__ ":"__LINE__, format, ##__VA_ARGS__)
#define XLOG_INFO_EXPL(chan_id, tgt_id, tag, format, ...)                      \
    g_log->write_fl(xlog::level::info, chan_id, tgt_it, tag,                   \
                    __FILE__ ":"__LINE__, format, ##__VA_ARGS__)
#define XLOG_DEBUG_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write_fl(xlog::level::debug, chan_id, tgt_it, tag,                  \
                    __FILE__ ":"__LINE__, format, ##__VA_ARGS__)
#define XLOG_TRACE_EXPL(chan_id, tgt_id, tag, format, ...)                     \
    g_log->write_fl(xlog::level::trace, chan_id, tgt_it, tag,                  \
                    __FILE__ ":"__LINE__, format, ##__VA_ARGS__)

#endif // XLOG_FILE_LINE

#else // X3ME_TEST

template <typename... Args>
void silence_warnings(Args&&...) noexcept
{
}

#define XLOG_FATAL(...) silence_warnings(__VA_ARGS__)
#define XLOG_ERROR(...) silence_warnings(__VA_ARGS__)
#define XLOG_WARN(...) silence_warnings(__VA_ARGS__)
#define XLOG_INFO(...) silence_warnings(__VA_ARGS__)
#define XLOG_DEBUG(...) silence_warnings(__VA_ARGS__)
#define XLOG_TRACE(...) silence_warnings(__VA_ARGS__)

#define XLOG_FATAL_EXPL(...) silence_warnings(__VA_ARGS__)
#define XLOG_ERROR_EXPL(...) silence_warnings(__VA_ARGS__)
#define XLOG_WARN_EXPL(...) silence_warnings(__VA_ARGS__)
#define XLOG_INFO_EXPL(...) silence_warnings(__VA_ARGS__)
#define XLOG_DEBUG_EXPL(...) silence_warnings(__VA_ARGS__)
#define XLOG_TRACE_EXPL(...) silence_warnings(__VA_ARGS__)

#endif // X3ME_TEST
