#pragma once

struct xproxy_stats
{
    // Can be used to find out if the algorithm for connections distribution
    // works well or not.
    uint64_t all_cnt_distrib_conns_ = 0;
    uint64_t max_cnt_distrib_conns_ = 0;
    uint64_t avg_cnt_distrib_conns_ = 0;
    uint64_t min_cnt_distrib_conns_ = 0;
};
