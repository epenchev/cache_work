#include "precompiled.h"
#include "plugins_mgr.h"
#include "http/http_trans.h"
#include "settings.h"

namespace plgns
{

plugins* plugins::instance = nullptr;

plugins_mgr::plugins_mgr() noexcept : ios_work_(ios_), host_stats_(ios_)
{
    plugins::instance = this;
}

plugins_mgr::~plugins_mgr() noexcept
{
    plugins::instance = nullptr;
    ios_.stop();
    if (work_thread_.joinable())
        work_thread_.join();
}

bool plugins_mgr::init(const settings& sts,
                       const net_thread_exec& net_exec) noexcept
{
    const char* curr_plugin = "";
    auto init_plugin = [&](auto& plugin, const char* name,
                           const std::string& cfg_path, auto&&... plugin_args)
    {
        curr_plugin = name;
        if (cfg_path.empty())
        {
            XLOG_INFO(plgn_tag, "Skip initializing '{}' plugin", name);
            return false;
        }
        std::ifstream ifs(cfg_path);
        if (ifs.fail())
        {
            throw std::invalid_argument("Unable to open config file: " +
                                        cfg_path);
        }
        plugin.init(ifs, plugin_args...);
        XLOG_INFO(plgn_tag, "Initialized '{}' plugin", name);
        return true;
    };
    bool run_ios = false;
    try
    {
        init_plugin(cache_url_, "cache_url", sts.plugins_cache_url_cfg());
        run_ios = init_plugin(host_stats_, "host_stats",
                              sts.plugins_host_stats_cfg(), net_exec);
    }
    catch (const std::exception& ex)
    {
        XLOG_ERROR(plgn_tag, "Unable to initialize '{}' plugin. {}",
                   curr_plugin, ex.what());
        return false;
    }
    if (run_ios)
    {
        work_thread_ = std::thread(
            [this]
            {
                x3me::sys_utils::set_this_thread_name("xproxy_plgns");
                ios_.run();
            });
    }
    return true;
}

void plugins_mgr::on_before_cache_open_read(net_thread_id_t,
                                            http::http_trans& trans) noexcept
{
    boost_string_t cache_url;
    const auto orig_url = trans.req_url();
    cache_url_.produce_cache_url(orig_url, cache_url);
    if (!cache_url.empty())
    {
        XLOG_INFO(trans.tag(), "Set cache_URL: {}. Orig_URL: {}", cache_url,
                  orig_url);
        trans.set_cache_url(std::move(cache_url));
    }
}

void plugins_mgr::on_transaction_end(net_thread_id_t net_tid,
                                     http::http_trans& trans) noexcept
{
    const auto msg_bytes = trans.resp_bytes();
    // Don't process hosts for empty responses and/or invalid transactions
    if ((msg_bytes > 0) && trans.is_valid())
    {
        const auto url       = trans.req_url();
        const auto org_bytes = trans.origin_resp_bytes();
        // If this is miss transaction the msg_bytes are equal to org_bytes
        // and thus the hit_bytes are 0.
        const auto hit_bytes = msg_bytes - org_bytes;
        const bool res =
            host_stats_.record_host(net_tid, url, hit_bytes, org_bytes);
        if (X3ME_UNLIKELY(!res))
        {
            XLOG_ERROR(trans.tag(), "Failed to record host stats for URL '{}'",
                       url);
        }
    }
}

} // namespace plgns
