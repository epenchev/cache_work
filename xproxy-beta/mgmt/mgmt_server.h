#pragma once

class settings;

namespace net
{
struct all_stats;
} // namespace net
namespace http
{
struct var_stats;
struct resp_size_stats;
} // namespace http
namespace cache
{
struct stats_fs;
struct stats_internal;
} // namespace cache
////////////////////////////////////////////////////////////////////////////////
namespace mgmt
{

using summary_net_stats_cb_t    = std::function<void(net::all_stats&&)>;
using summary_http_stats_cb_t   = std::function<void(http::var_stats&&)>;
using resp_size_http_stats_cb_t = std::function<void(http::resp_size_stats&&)>;
using cache_stats_cb_t = std::function<void(std::vector<cache::stats_fs>&&)>;
using cache_internal_stats_cb_t =
    std::function<void(std::vector<cache::stats_internal>&&)>;

class mgmt_server
{
    io_service_t ios_;
    std::thread runner_;
    x3me::net::json_rpc::server impl_;
    const settings& settings_;

    template <typename T>
    using mem_fn_delegate_t = x3me::utils::mem_fn_delegate<T>;

public:
    mem_fn_delegate_t<void(const summary_net_stats_cb_t&)> fn_summary_net_stats;
    mem_fn_delegate_t<void(const summary_http_stats_cb_t&)>
        fn_summary_http_stats;
    mem_fn_delegate_t<void(const resp_size_http_stats_cb_t&)>
        fn_resp_size_http_stats;
    mem_fn_delegate_t<void(const cache_stats_cb_t&)> fn_cache_stats;
    mem_fn_delegate_t<void(const cache_internal_stats_cb_t&)>
        fn_cache_internal_stats;

public:
    explicit mgmt_server(const settings& sts) noexcept;
    ~mgmt_server() noexcept;

    mgmt_server() = delete;
    mgmt_server(const mgmt_server&) = delete;
    mgmt_server& operator=(const mgmt_server&) = delete;
    mgmt_server(mgmt_server&&) = delete;
    mgmt_server& operator=(mgmt_server&&) = delete;

    bool start(const ip_addr4_t& bind_ip, uint16_t bind_port) noexcept;
    void stop() noexcept;

private:
    void subscribe() noexcept;

    using jr_res = x3me::net::json_rpc::json_rpc_res;

    static void respond_debug_cmd(jr_res&& res, string_view_t ret) noexcept;
    // The signature of these functions must be the following:
    // - First parameter always jr_res&&.
    // - Next parameters exactly the same as the corresponding callback
    // type defined above at the namespace mgmt level.
    static void respond_summary_net_stats(jr_res&& res,
                                          net::all_stats&& as) noexcept;
    static void respond_summary_http_stats(jr_res&& res,
                                           http::var_stats&& as) noexcept;
    static void
    respond_resp_size_http_stats(jr_res&& res,
                                 http::resp_size_stats&& st) noexcept;
    static void
    respond_summary_cache_stats(jr_res&& res,
                                std::vector<cache::stats_fs>&& st) noexcept;
    static void
    respond_detailed_cache_stats(jr_res&& res,
                                 std::vector<cache::stats_fs>&& st) noexcept;
    static void respond_summary_internal_cache_stats(
        jr_res&& res, std::vector<cache::stats_internal>&& st) noexcept;

    template <typename... Args>
    std::function<void(Args...)>
    make_cb(const jr_res& res, void (*fn)(jr_res&&, Args...)) noexcept;
};

} // namespace mgmt
