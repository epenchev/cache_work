#pragma once

#include "cache_url.h"
#include "host_stats.h"
#include "plugins.h"

class settings;

namespace plgns
{

class plugins_mgr final : private plugins
{
    io_service_t ios_;
    io_service_t::work ios_work_;
    std::thread work_thread_;

    cache_url cache_url_;
    host_stats host_stats_;

public:
    plugins_mgr() noexcept;
    ~plugins_mgr() noexcept final;

    plugins_mgr(const plugins_mgr&) = delete;
    plugins_mgr& operator=(const plugins_mgr&) = delete;
    plugins_mgr(plugins_mgr&&) = delete;
    plugins_mgr& operator=(plugins_mgr&&) = delete;

    bool init(const settings& sts, const net_thread_exec& net_exec) noexcept;

    // The functions here can be called by multiple threads in the same time
    void on_before_cache_open_read(net_thread_id_t,
                                   http::http_trans& trans) noexcept final;
    void on_transaction_end(net_thread_id_t net_tid,
                            http::http_trans& trans) noexcept final;
};

} // namespace plgns
