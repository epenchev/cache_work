#pragma once

#include "http_trans.h"
#include "cache/async_stream.h"
#include "net/proto_handler.h"
#include "xutils/io_buff_reader.h"

namespace cache
{
struct cache_key;
} // namespace cache
namespace http
{
struct all_stats;
class http_bp_ctl;
namespace detail
{
enum client_rbuf_size_idx : uint8_t;
enum origin_rbuf_size_idx : uint8_t;
namespace hhsm
{
struct sm;
struct sm_impl;
} // namespace hhsm
////////////////////////////////////////////////////////////////////////////////

class http_handler final : public net::proto_handler
{
    // A small_vector container is much more memory efficient structure,
    // than std::queue, if we assume that more than 99% of the cases we won't
    // have HTTP pipelining. The expectation is that even in keep alive
    // connections we won't have more than 1 request in flight in any given
    // time. We can support more requests in flight, with the current container,
    // but it won't be so efficient.
    using transactions_t = boost::container::small_vector<http_trans, 1>;

    xutils::io_buff_reader client_rdr_;
    xutils::io_buff_reader origin_rdr_;
    xutils::io_buff_reader org_cache_rdr_; // Origin to cache reader

    cache::async_stream cache_handle_;

    x3me::utils::pimpl<hhsm::sm, 64, 8> csm_; // The cache logic state machine

    transactions_t transactions_;

    all_stats& all_stats_;

    http_bp_ctl& bp_ctrl_;

    id_tag tag_;
    id_tag::trans_id_t cln_trans_id_ = 1;
    id_tag::trans_id_t org_trans_id_ = 1;

    detail::client_rbuf_size_idx client_rbuf_size_idx_;
    detail::origin_rbuf_size_idx origin_rbuf_size_idx_;

    const net_thread_id_t net_thread_id_;

    using flags_t = uint8_t;
    enum flags : flags_t
    {
        bpctrl_entry_added    = 1 << 0,
        pending_blind_tunnel  = 1 << 1,
        origin_recv_paused    = 1 << 2,
        transaction_completed = 1 << 3,
        // These flags are valid only in the context of the current transaction.
        // We reset them when the current transaction gets removed.
        tr_bpctrl_params_set = 1 << 4,
        tr_caching_started   = 1 << 5,
        tr_after_end_data    = 1 << 6,
    };
    flags_t flags_ = 0;

public:
    http_handler(const id_tag& tag,
                 cache::object_distributor& cod,
                 io_service_t& ios,
                 net_thread_id_t net_tid,
                 all_stats& sts,
                 http_bp_ctl& bp_ctl) noexcept;
    ~http_handler() noexcept final;

    http_handler(const http_handler&) = delete;
    http_handler& operator=(const http_handler&) = delete;
    http_handler(http_handler&&) = delete;
    http_handler& operator=(http_handler&&) = delete;

    void init(net::proxy_conn& conn) noexcept final;

    void on_origin_pre_connect(net::proxy_conn& conn) noexcept final;

    void on_switched_stream_eof(net::proxy_conn& conn) noexcept final;

    void on_client_data(net::proxy_conn& conn) noexcept final;
    void on_client_recv_eof(net::proxy_conn& conn) noexcept final;
    void on_client_recv_err(net::proxy_conn& conn) noexcept final;
    void on_client_send_err(net::proxy_conn& conn) noexcept final;

    void on_origin_data(net::proxy_conn& conn) noexcept final;
    void on_origin_recv_eof(net::proxy_conn& conn) noexcept final;
    void on_origin_recv_err(net::proxy_conn& conn) noexcept final;
    void on_origin_send_err(net::proxy_conn& conn) noexcept final;

private:
    // A response is expected if a request already has been sent
    bool is_resp_expected(net::proxy_conn& conn) noexcept;
    bool consume_curr_resp_data(net::proxy_conn& conn) noexcept;
    void adjust_trans_state(net::proxy_conn& conn,
                            http_trans& curr_trans,
                            bool completed_at_once) noexcept;
    bool set_curr_trans_backpressure_params(net::proxy_conn& conn) noexcept;
    bool set_bpctrl_content_len(bytes64_t clen) noexcept;
    void set_bpctrl_curr_trans_clen() noexcept;
    bool process_curr_trans_resp(net::proxy_conn& conn) noexcept;

private:
    friend struct hhsm::sm_impl;
    // Methods called by the state machine
    bool has_cache_wr_data() const noexcept;
    bool trans_completed() const noexcept;
    void set_trans_completed() noexcept;
    bool pend_blind_tunnel() const noexcept;
    void set_pend_blind_tunnel() noexcept;
    void start_blind_tunnel(net::proxy_conn& conn) noexcept;
    void switch_org_stream(net::proxy_conn& conn) noexcept;
    void pause_org_recv(net::proxy_conn& conn) noexcept;
    void resume_org_recv(net::proxy_conn& conn) noexcept;
    void fin_trans_send_next(net::proxy_conn& conn) noexcept;
    void consume_cache_hdrs_data() noexcept;
    void consume_cache_data() noexcept;
    void cache_open_rd(net::proxy_conn& conn) noexcept;
    void cache_read_compare(net::proxy_conn& conn) noexcept;
    void cache_reopen_wr_truncate(net::proxy_conn& conn) noexcept;
    void cache_open_wr(net::proxy_conn& conn, bool truncate_obj) noexcept;
    void cache_write(net::proxy_conn& conn) noexcept;
    void consume_cache_wr_data(net::proxy_conn& conn, bytes32_t len) noexcept;
    void cache_close() noexcept;
    void rem_bpctrl_entry() noexcept;

private:
    void reset_per_trans_flags() noexcept;

    void expand_client_recv_buff_if_needed(const http_trans& trans,
                                           net::proxy_conn& conn) noexcept;
    void expand_origin_recv_buff_if_needed(const http_trans& trans,
                                           net::proxy_conn& conn) noexcept;

    const id_tag& no_trans_tag() noexcept;
    const id_tag& cln_trans_tag() noexcept;
    const id_tag& org_trans_tag() noexcept;
};

} // namespace detail
} // namespace http
