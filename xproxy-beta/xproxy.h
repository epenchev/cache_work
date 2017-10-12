#pragma once

#include "mgmt/mgmt_server.h"
#include "cache/cache_mgr.h"
#include "http/http_stats.h"
#include "http/http_bp_ctl.h"
#include "net/net_stats.h"
#include "plgns/plugins_mgr.h"

class settings;

class xproxy
{
    using ios_work_ptr_t = std::unique_ptr<io_service_t::work>;

    struct worker_stats
    {
        http::all_stats http_stats_;
        net::all_stats net_stats_;
    };

    struct net_worker
    {
        worker_stats stats_;
        http::http_bp_ctl bp_ctrl_;

        // The async objects living inside the io_service_t needs to be
        // destroyed before above members because they may be referencing
        // some of them in their destructors.
        io_service_t ios_;
        std_timer_t half_closed_tmr_; // Used to close inactive half closed

        net_worker() : half_closed_tmr_(ios_) {}
    };

    const settings& settings_;

    io_service_t main_ios_;
    tcp_acceptor_t acceptor_;
    signal_set_t signal_set_;

    // The cache manager object needs to outlive the objects living inside
    // the net_services. It keeps alive objects used internally by the
    // async_file_stream objects. Thus the async_file_stream objects needs
    // to be destroyed before the cache manager. The async_file_stream objects
    // may be kept alive by objects living inside the net_services.
    cache::cache_mgr cache_mgr_;

    plgns::plugins_mgr plugins_mgr_;

    std::vector<net_worker> net_workers_;
    net_thread_id_t curr_net_worker_ = 0; // To post the next connection

    id_tag::sess_id_t curr_session_id_ = 1;

    mgmt::mgmt_server mgmt_server_;

public:
    explicit xproxy(const settings& sts) noexcept;
    ~xproxy() noexcept;

    xproxy() = delete;
    xproxy(const xproxy&) = delete;
    xproxy& operator=(const xproxy&) = delete;
    xproxy(xproxy&&) = delete;
    xproxy& operator=(xproxy&&) = delete;

    // This function will never return if the fast_exit flag is set to true
    bool run(bool reset_cache, bool fast_exit) noexcept;

private:
    bool init() noexcept;
    void run_impl(bool fast_exit) noexcept;

    bool init_plugins_mgr() noexcept;
    bool init_mgmt_server() noexcept;
    bool relinquish_privileges() noexcept;
    bool setup_acceptor() noexcept;
    void accept_connection() noexcept;
    void distribute_connection(tcp_socket_t&& s) noexcept;
    void schedule_check_half_closed(net_thread_id_t net_tid) noexcept;
    void check_half_closed(net_thread_id_t net_tid) noexcept;
    void handle_sys_signal() noexcept;
    void set_subsystems_settings() noexcept;

private:
    // Callbacks functions called by the management server.
    // Note that hey are called from the management thread, internal to the
    // management server.
    void
    mgmt_cb_summary_net_stats(const mgmt::summary_net_stats_cb_t& cb) noexcept;
    void mgmt_cb_summary_http_stats(
        const mgmt::summary_http_stats_cb_t& cb) noexcept;
    void mgmt_cb_resp_size_http_stats(
        const mgmt::resp_size_http_stats_cb_t& cb) noexcept;
    void mgmt_cb_cache_stats(const mgmt::cache_stats_cb_t& cb) noexcept;
    void mgmt_cb_cache_internal_stats(
        const mgmt::cache_internal_stats_cb_t& cb) noexcept;
};
