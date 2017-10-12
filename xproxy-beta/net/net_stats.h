#pragma once

namespace net
{

struct all_stats
{
    uint64_t cnt_connect_success_ = 0;
    uint64_t cnt_connect_failed_  = 0;

    bytes64_t bytes_all_client_recv_ = 0;
    bytes64_t bytes_all_origin_recv_ = 0;
    bytes64_t bytes_all_origin_send_ = 0;
    bytes64_t bytes_all_client_send_ = 0;
    bytes64_t bytes_hit_client_send_ = 0;

    uint64_t cnt_half_closed_            = 0;
    uint64_t cnt_half_closed_cln_recv_   = 0;
    uint64_t cnt_half_closed_org_recv_   = 0;
    uint64_t cnt_half_closed_cln_closed_ = 0;
    uint64_t cnt_half_closed_org_closed_ = 0;
    uint64_t cnt_closed_half_closed_     = 0;
    uint32_t cnt_curr_half_closed_       = 0;

    uint32_t cnt_curr_conns_        = 0;
    uint32_t cnt_curr_connecting_   = 0;
    uint32_t cnt_curr_blind_tunnel_ = 0;

    all_stats& operator+=(const all_stats& rhs) noexcept
    {
        // clang-format off
        cnt_connect_success_        += rhs.cnt_connect_success_;
        cnt_connect_failed_         += rhs.cnt_connect_failed_;
        bytes_all_client_recv_      += rhs.bytes_all_client_recv_;
        bytes_all_origin_recv_      += rhs.bytes_all_origin_recv_;
        bytes_all_origin_send_      += rhs.bytes_all_origin_send_;
        bytes_all_client_send_      += rhs.bytes_all_client_send_;
        bytes_hit_client_send_      += rhs.bytes_hit_client_send_;
        cnt_half_closed_            += rhs.cnt_half_closed_;
        cnt_half_closed_cln_recv_   += rhs.cnt_half_closed_cln_recv_;
        cnt_half_closed_org_recv_   += rhs.cnt_half_closed_org_recv_;
        cnt_half_closed_cln_closed_ += rhs.cnt_half_closed_cln_closed_;
        cnt_half_closed_org_closed_ += rhs.cnt_half_closed_org_closed_;
        cnt_closed_half_closed_     += rhs.cnt_closed_half_closed_;
        cnt_curr_half_closed_       += rhs.cnt_curr_half_closed_;
        cnt_curr_conns_             += rhs.cnt_curr_conns_;
        cnt_curr_connecting_        += rhs.cnt_curr_connecting_;
        cnt_curr_blind_tunnel_      += rhs.cnt_curr_blind_tunnel_;
        // clang-format on
        return *this;
    }
};

} // namespace net
