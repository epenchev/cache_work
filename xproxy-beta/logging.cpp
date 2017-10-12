#include "precompiled.h"
#include "logging.h"
#include "settings.h"
#include "debug_filter.h"
#include "xlog/log_target.h"
#include "xlog/logger.ipp"
#include "xproxy_ctl/debug_cmd.h"

namespace fs = boost::filesystem;
namespace pt = boost::posix_time;

// Instantiate explicitly the logger type in order to speed the compilation
template class xlog::logger<id_tag>;

////////////////////////////////////////////////////////////////////////////////
namespace
{
uint16_t max_records_soft_lim(uint16_t hard_lim) noexcept
{
    if (hard_lim < 100)
        return hard_lim - 10;
    else if (hard_lim < 200)
        return hard_lim - 20;
    else if (hard_lim < 500)
        return hard_lim - 35;
    return hard_lim - 50;
}

uint32_t slide_tolerance_bytes(uint64_t file_max_bytes) noexcept
{
    // Get the min value of 128 MB or 10% of file_max_bytes.
    constexpr uint64_t tmp = 10 * 128_MB;
    if (file_max_bytes > tmp)
        return 128_MB;
    // The slide tolerance is checked internally against the real
    // io_block_size but so far it's been always 4KB.
    constexpr uint32_t io_block_size = 4_KB;
    return x3me::math::round_up_pow2((file_max_bytes * 10) / 100,
                                     io_block_size);
}

////////////////////////////////////////////////////////////////////////////////
// N.B. Some of these helper functions may throw in some weird situations.
// However, these situations should never happen in practice and thus I
// don't want to complicate the code without need.
// Setting the functions as noexcept will invoke std::terminate if some
// of these functions throws. Let's see if I was wrong doing this :)

fs::path main_log_filepath(const fs::path& logs_dir,
                           const pt::ptime& t) noexcept
{
    const auto fname = pt::to_iso_string(t);
    return logs_dir / (fname + ".log");
}

fs::path curr_main_log_filepath(const fs::path& logs_dir) noexcept
{
    return main_log_filepath(logs_dir, pt::second_clock::local_time());
}

fs::path curr_debg_log_filepath(const fs::path& logs_dir) noexcept
{
    using namespace boost;
    const auto now   = posix_time::second_clock::local_time();
    const auto fname = to_iso_string(now);
    return logs_dir / (fname + ".debg.log");
}

// Returns vector with the timestamps, obtained from the log names of
// the main log files in the given directory.
// The returned timestamps are sorted from oldest to the newest.
std::vector<pt::ptime> get_curr_main_logs(const fs::path& logs_dir) noexcept
{
    std::vector<pt::ptime> res;
    try
    {
        auto end_it = fs::directory_iterator();
        for (auto it = fs::directory_iterator(logs_dir); it != end_it; ++it)
        {
            err_code_t err;
            auto st = it->status(err);
            if (err)
                continue;
            const auto& p = it->path();
            if ((st.type() != fs::regular_file) || (p.extension() != ".log"))
                continue;
            const auto fn = p.stem(); // Remove the .log extension
            // Don't use it if it is a debug log - <ISO8601datetime>.debg.log
            if (fn.extension() == ".debg")
                continue;
            try
            {
                // If the datetime parser fails, it throws exception.
                // There is no way around this. There will be also
                // squid.log and probably some manually copied logs
                // with all kind of names.
                res.push_back(pt::from_iso_string(fn.native()));
            }
            catch (...)
            {
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Unable to check the logs in " << logs_dir << ". "
                  << ex.what() << "'\n";
    }
    // Sort them so the newest log date time will be last.
    std::sort(res.begin(), res.end());
    return res;
}

std::vector<fs::path> get_all_logs(const fs::path& logs_dir)
{
    std::vector<fs::path> res;
    auto end_it = fs::directory_iterator();
    for (auto it = fs::directory_iterator(logs_dir); it != end_it; ++it)
    {
        err_code_t err;
        auto st = it->status(err);
        if (err)
            continue;
        const auto& p = it->path();
        if ((st.type() != fs::regular_file) || (p.extension() != ".log"))
            continue;
        auto fn = p.stem(); // Remove the .log extension
        if (fn == "squid")
        {
            res.push_back(p);
            continue;
        }
        // Don't use it if it is a debug log - <ISO8601datetime>.debg.log
        if (fn.extension() == ".debg")
            fn = fn.stem();
        try
        {
            // If the datetime parser fails, it throws exception.
            // There is no way around this. The call to 'is_not_a_date_time'
            // is not actually needed, because the conversion will throw if
            // the string is invalid.
            if (!pt::from_iso_string(fn.native()).is_not_a_date_time())
            {
                res.push_back(p);
            }
        }
        catch (...)
        {
        }
    }
    return res;
}

////////////////////////////////////////////////////////////////////////////////

fs::path clear_logs_get_curr_log(const settings& sts) noexcept
{

    pt::ptime curr_log_time = pt::second_clock::local_time();
    try
    {
        auto main_logs = get_curr_main_logs(sts.log_logs_dir());
        if (!main_logs.empty())
        {
            if (main_logs.back().date() == curr_log_time.date())
            {
                // There is a log from the same day as today.
                // We'll continue to use it.
                curr_log_time = main_logs.back();
                main_logs.pop_back();
            }
        }
        const auto max_cnt = sts.log_main_log_rotate_count();
        if (main_logs.size() > max_cnt)
        {
            // Remove the redundant logs
            const uint32_t to_rem = main_logs.size() - max_cnt;
            for (uint32_t i = 0; i < to_rem; ++i)
            {
                err_code_t err;
                const auto fp =
                    main_log_filepath(sts.log_logs_dir(), main_logs[i]);
                fs::remove(fp, err);
                if (err)
                {
                    std::cerr << "Unable to remove redundant log '" << fp
                              << "' on startup\n";
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr
            << "General failure when clearing redundant log files on startup. "
            << ex.what() << '\n';
    }
    return main_log_filepath(sts.log_logs_dir(), curr_log_time);
}

// This callback function is expected to return the filepath of the new log.
// Note that it's executed in the context of the logging thread.
std::string on_main_log_rotate(const std::string& curr_log_path,
                               uint16_t rotate_cnt) noexcept
{
    const fs::path logs_dir = fs::path(curr_log_path).parent_path();
    try
    {
        auto main_logs = get_curr_main_logs(logs_dir);
        if (main_logs.size() > rotate_cnt)
        {
            // Remove the redundant logs
            const uint32_t to_rem = main_logs.size() - rotate_cnt;
            for (uint32_t i = 0; i < to_rem; ++i)
            {
                err_code_t err;
                const auto fp = main_log_filepath(logs_dir, main_logs[i]);
                fs::remove(fp, err);
                if (err)
                {
                    std::cerr << "Unable to remove redundant log '" << fp
                              << "' on rotate\n";
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr
            << "General failure when clearing redundant log files on rotate. "
            << ex.what() << '\n';
    }
    return curr_main_log_filepath(logs_dir).string();
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

logger_ptr_t g_log = xlog::create_logger<id_tag>();

bool init_logging(const settings& sts) noexcept
{
    using namespace xlog;

    constexpr bool truncate      = false;
    const fs::path logs_dir      = sts.log_logs_dir();
    const fs::path main_log_path = clear_logs_get_curr_log(sts);

    const auto rotate_cnt = sts.log_main_log_rotate_count();
    auto on_main_log_rotate_wrap =
        [rotate_cnt](const std::string& curr_log_path)
    {
        return on_main_log_rotate(curr_log_path, rotate_cnt);
    };

    err_code_t err;

    // Create the target for the main log
    uint64_t max_bytes = sts.log_main_log_rotate_MB() * 1024 * 1024;
    auto main_log = create_file_rotate_target(
        main_log_path.c_str(), truncate, max_bytes, sts.log_main_log_level(),
        on_main_log_rotate_wrap, err);
    if (err)
    {
        std::cerr << "Unable to create the main log target. File_path '"
                  << main_log_path << "'. " << err.message() << '\n';
        return false;
    }

    // Create the squid log target
    max_bytes                     = sts.log_squid_log_slide_MB() * 1024 * 1024;
    const fs::path squid_log_path = logs_dir / "squid.log";
    auto squid_log = create_file_sliding_target(
        squid_log_path.c_str(), max_bytes, slide_tolerance_bytes(max_bytes),
        xlog::level::info, err);
    if (err)
    {
        std::cerr << "Unable to create the squid log target. File_path '"
                  << squid_log_path << "'. " << err.message() << '\n';
        return false;
    }

    // Create the syslog/dmesg target
    auto dmesg = create_syslog_target(sts.log_sys_log_level(), err);
    if (err)
    {
        std::cerr << "Unable to create the syslog/dmesg target. "
                  << err.message() << '\n';
        return false;
    }

    // Assign the targets to their asynchronous channel
    const auto hard_lim = sts.log_max_pending_records();
    auto chan = create_async_channel("xproxy_log", hard_lim,
                                     max_records_soft_lim(hard_lim));
    bool res = chan.add_log_target(main_log_id, std::move(main_log));
    X3ME_ENFORCE(res); // Must succeed
    // The messages to the squid blog will be sent explicitly
    res = chan.add_explicit_log_target(squid_log_id, std::move(squid_log));
    X3ME_ENFORCE(res); // Must succeed
    res = chan.add_log_target(sys_log_id, std::move(dmesg));
    X3ME_ENFORCE(res); // Must succeed

    // Finally add the asynchronous channel with its targets to the logger.
    res = g_log->add_async_channel(main_log_chan, std::move(chan));
    X3ME_ENFORCE(res); // Must succeed

    return true;
}

void chown_logs(const std::string& logs_dir, uid_t user_id, gid_t group_id)
{
    namespace bsys = boost::system;
    std::vector<fs::path> logs;
    try
    {
        logs = get_all_logs(logs_dir);
    }
    catch (const fs::filesystem_error& err)
    {
        throw bsys::system_error(err.code(), "Unable get all log filepaths");
    }
    if (::chown(logs_dir.c_str(), user_id, group_id) != 0)
    {
        throw bsys::system_error(errno, bsys::get_system_category(),
                                 "Unable to change owner of the logs dir '" +
                                     logs_dir + "'");
    }
    constexpr mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    for (const auto& l : logs)
    {
        if (::chown(l.c_str(), user_id, group_id) != 0)
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to change owner of log file '" +
                                         l.native() + "'");
        }
        if (::chmod(l.c_str(), mode) != 0)
        {
            throw bsys::system_error(
                errno, bsys::get_system_category(),
                "Unable to change permissions of log file '" + l.native() +
                    "'");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
namespace
{
using rt_debug_logs_t =
    boost::container::flat_map<std::string, xlog::channel_id>;
// The start/stop RT debugging is currently called only from single thread,
// but let's play safe for the future :).
static x3me::thread::synchronized<rt_debug_logs_t, std::mutex> rt_debug_logs_;

bool cmp_dlogs(const rt_debug_logs_t::value_type& lhs,
               const rt_debug_logs_t::value_type& rhs)
{ // Compare the channel ids
    return lhs.second.value() < rhs.second.value();
};

void add_unfiltered_channel(xlog::async_channel&& chan)
{
    with_synchronized(
        rt_debug_logs_, [&](rt_debug_logs_t& info)
        {
            auto it = std::max_element(info.cbegin(), info.cend(), cmp_dlogs);

            const auto chan_id = (it == info.cend())
                                     ? debg_log_chan
                                     : xlog::channel_id(it->second.value() + 1);
            assert(chan_id.value() >= debg_log_chan.value());
            assert(chan_id.value() < uint16_t(-1));
            auto res = info.emplace("", chan_id);
            if (!res.second)
            {
                throw std::logic_error("Debug log has been already started "
                                       "with empty filter.");
            }
            if (!g_log->add_async_channel(chan_id, std::move(chan)))
            {
                info.erase(res.first);
                throw std::logic_error("Unable to add new log. The "
                                       "limit of allowed debug logs has "
                                       "been reached.");
            }
        });
}

void add_filtered_channel(string_view_t cmd, xlog::async_channel&& chan)
{
    auto it  = cmd.cbegin();
    auto end = cmd.cend();

    // Parse the command and get the AST tree
    dcmd::ast::group_expr cmd_tree;
    boost::spirit::x3::ascii::space_type space; // Skip additional spaces
    if (!phrase_parse(it, end, dcmd::cmd_descr, space, cmd_tree))
    {
        throw std::logic_error("Invalid debug filter. Parsing stopped at: '" +
                               std::string(it, end - it) + '\'');
    }

    // Change the AST tree to a more effective one (or more defective :))
    dfilter::ast_convertor conv;
    dfilter::ast::filt_expr filt_tree = conv(cmd_tree);
    // If there is log level filter expression we set the lowest possible
    // as default, and then it'll get filtered from the expression.
    const auto def_log_level = conv.has_log_level_
                                   ? xlog::to_number(xlog::level::trace)
                                   : xlog::to_number(xlog::level::debug);

    // The highest level must be group expression, otherwise we have a bug
    // in the command parsing or/and converting.
    auto filter_fun = [ filt_tree = std::move(filt_tree), def_log_level ](
        xlog::level msg_lvl, const id_tag& msg_tag)
    {
        if (xlog::to_number(msg_lvl) <= def_log_level)
        {
            dfilter::filter f(msg_tag, msg_lvl);
            return boost::apply_visitor(f, filt_tree);
        }
        return false;
    };

    with_synchronized(
        rt_debug_logs_, [&](rt_debug_logs_t& info)
        {
            auto it = std::max_element(info.cbegin(), info.cend(), cmp_dlogs);

            const auto chan_id = (it == info.cend())
                                     ? debg_log_chan
                                     : xlog::channel_id(it->second.value() + 1);
            assert(chan_id.value() >= debg_log_chan.value());
            assert(chan_id.value() < uint16_t(-1));
            const std::string scmd(cmd.data(), cmd.size());
            auto r = info.emplace(scmd, chan_id);
            if (!r.second)
            {
                throw std::logic_error("Debug log has been already started "
                                       "with this filter '" +
                                       scmd + "'.");
            }
            if (!g_log->add_async_channel(chan_id, std::move(chan),
                                          std::move(filter_fun)))
            {
                info.erase(r.first);
                throw std::logic_error("Unable to add new log. The "
                                       "limit of allowed debug logs has "
                                       "been reached.");
            }
        });
}

} // namespace

bool start_rt_debug(const settings& sts, string_view_t cmd,
                    std::string& out_err) noexcept
{
    bool res            = true;
    const auto log_path = curr_debg_log_filepath(sts.log_logs_dir());
    try
    {
        err_code_t err;
        auto debg_log = create_file_target(log_path.c_str(), true /*truncate*/,
                                           xlog::level::trace, err);
        if (err)
        {
            throw std::runtime_error("Unable to create debug log '" +
                                     log_path.string() + "'. " + err.message() +
                                     '.');
        }
        const auto hard_lim = sts.log_max_pending_records();
        auto chan = xlog::create_async_channel("xproxy_dlog", hard_lim,
                                               max_records_soft_lim(hard_lim));
        bool res = chan.add_log_target(debg_log_id, std::move(debg_log));
        X3ME_ENFORCE(res); // Must succeed

        // Add debug channel without filter function if the command is empty.
        if (!cmd.empty())
            add_filtered_channel(cmd, std::move(chan));
        else
            add_unfiltered_channel(std::move(chan));
    }
    catch (const std::exception& ex)
    {
        // Try to remove the created file in case of error.
        // It's kind of confusing if the command fails but a new log file
        // gets created;
        err_code_t skip;
        fs::remove(log_path, skip);

        out_err = ex.what();
        res     = false;
    }
    return res;
}

bool stop_rt_debug(string_view_t cmd, std::string& out_err) noexcept
{
    bool res = true;
    using namespace x3me::thread;
    with_synchronized(rt_debug_logs_, [&](rt_debug_logs_t& info)
                      {
                          constexpr bool flush_log = false;
                          if (cmd.empty())
                          { // Remove all RT debug logs
                              if (!info.empty())
                              {
                                  for (const auto& i : info)
                                  {
                                      const bool r = g_log->rem_async_channel(
                                          i.second, flush_log);
                                      // The channel is present in the
                                      // rt_debug_logs
                                      // i.e. it must be present in the logger
                                      // too.
                                      X3ME_ENFORCE(r);
                                  }
                                  info.clear();
                              }
                              else
                              {
                                  out_err = "No active debug logs";
                                  res     = false;
                              }
                          }
                          else
                          { // Remove only the corresponding RT debug log
                              const std::string scmd(cmd.data(), cmd.size());
                              auto it = info.find(scmd);
                              if (it != info.end())
                              {
                                  const bool r = g_log->rem_async_channel(
                                      it->second, flush_log);
                                  // The channel is present in the rt_debug_logs
                                  // i.e. it must be present in the logger too.
                                  X3ME_ENFORCE(r);
                                  info.erase(it);
                              }
                              else
                              {
                                  out_err = "No corresponding debug log";
                                  res     = false;
                              }
                          }
                      });
    return res;
}
