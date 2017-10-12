#pragma once

#include "xbpctrl/nf_bpctrl_nl.h"
#include "xbpctrl/usrland/netlink_conn.h"

namespace http
{

/// Interface over the netlink connection
/// for sending commands to back pressure control kernel module.
class http_bp_ctl
{
    netlink_conn nlconn_;

public:
    using err_code_t = netlink_conn::err_code_t;

    http_bp_ctl() noexcept = default;
    ~http_bp_ctl() noexcept = default;

    err_code_t init() noexcept { return nlconn_.bind_socket(); }

    err_code_t add_del_entry(const tcp_endpoint_v4& cl_epoint,
                             const tcp_endpoint_v4& rm_epoint) noexcept
    {
        auto cl_ip_be   = htonl(cl_epoint.address().to_ulong());
        auto cl_port_be = htons(cl_epoint.port());
        auto rm_ip_be   = htonl(rm_epoint.address().to_ulong());
        return nlconn_.send_cmd({0, cl_ip_be, rm_ip_be, cl_port_be, ADD_DEL_OP});
    }

    err_code_t content_len(uint64_t cont_len,
                           const tcp_endpoint_v4& cl_epoint,
                           const tcp_endpoint_v4& rm_epoint) noexcept
    {
        auto cl_ip_be   = htonl(cl_epoint.address().to_ulong());
        auto cl_port_be = htons(cl_epoint.port());
        auto rm_ip_be   = htonl(rm_epoint.address().to_ulong());
        return nlconn_.send_cmd({cont_len, cl_ip_be, rm_ip_be, cl_port_be, LEN_OP});
    }

    err_code_t chunked_end(const tcp_endpoint_v4& cl_epoint,
                           const tcp_endpoint_v4& rm_epoint) noexcept
    {
        auto cl_ip_be   = htonl(cl_epoint.address().to_ulong());
        auto cl_port_be = htons(cl_epoint.port());
        auto rm_ip_be   = htonl(rm_epoint.address().to_ulong());
        return nlconn_.send_cmd({0, cl_ip_be, rm_ip_be, cl_port_be, CHK_END_OP});
    }
};

} // namespace http
