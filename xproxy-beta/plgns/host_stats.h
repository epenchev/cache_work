#pragma once

#include "plgns_common.h"

namespace plgns
{

class host_stats
{
    struct counters
    {
        bytes64_t bytes_hit_  = 0;
        bytes64_t bytes_miss_ = 0;
        uint32_t reqs_hit_    = 0;
        uint32_t reqs_miss_   = 0;
    };
    // We can use sparse hash map as it is more space efficient
    using host_stats_t     = std::unordered_map<std::string, counters>;
    using host_stats_arr_t = std::vector<host_stats_t>;

    host_stats_t all_stats_;
    host_stats_arr_t thread_stats_; // For each networking thread

    net_thread_executor_t net_thread_exec_;

    struct settings
    {
        std::string dump_dir_;
        std::chrono::seconds dump_timeout_{300};
        // 3 means aaa.bbb.xxx.host.com will be stored as xxx.host.com - 3 parts
        uint16_t domain_level_ = 3;
    } settings_;

    std_timer_t dump_tmr_;

    std::string curr_dump_fpath_;
    int curr_day_ = -1;

public:
    explicit host_stats(io_service_t& ios) noexcept;
    ~host_stats() noexcept;

    host_stats(const host_stats&) = delete;
    host_stats& operator=(const host_stats&) = delete;
    host_stats(host_stats&&) = delete;
    host_stats& operator=(host_stats&&) = delete;

    void init(std::istream& cfg_data, const net_thread_exec& net_exec);

    // Even the hit transactions has some miss bytes - the headers for sure,
    // and part of the body in most of the cases.
    bool record_host(net_thread_id_t net_tid,
                     const string_view_t& url,
                     bytes64_t hit_bytes,
                     bytes64_t miss_bytes) noexcept;

private:
    void load_settings(std::istream& cfg);
    void load_curr_stats();

    void schedule_stats_dump() noexcept;
    void do_async_stats_dump() noexcept;
    void do_stats_dump(const host_stats_arr_t& stats) noexcept;

    void set_curr_dump_fpath(const std::tm& ctm) noexcept;

    static std::tm current_time() noexcept;
};

} // namespace plgns
