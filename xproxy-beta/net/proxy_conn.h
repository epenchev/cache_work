#pragma once

#include "async_read_stream.h"
#include "public_types.h"
#include "switchable_read_stream.h"
#include "xutils/io_buff.h"
#include "xutils/io_buff_reader.h"

namespace net
{
struct all_stats;
namespace pcsm
{
struct sm;
struct sm_impl;
} // namespace pcsm
////////////////////////////////////////////////////////////////////////////////
class proxy_conn;
using proxy_conn_ptr_t = boost::intrusive_ptr<proxy_conn>;

// Save 8 bytes from the std::shared_ptr weak counter, which is not needed.
class proxy_conn
    : public boost::intrusive_ref_counter<proxy_conn,
                                          boost::thread_safe_counter>
{
    using hook_t = boost::intrusive::list_member_hook<
        boost::intrusive::link_mode<boost::intrusive::safe_link>>;

    friend struct pcsm::sm_impl;

    struct bytes_cnt
    {
        // Yes a session can receive more than 2^32 bytes but we'll be using
        // these currently only to check if the session is stalled.
        // I do not expect between to checks for stalled the counters to
        // overflow to exact same numbers. So let's save some memory.
        bytes32_t org_recv_bytes_ = 0;
        bytes32_t cln_recv_bytes_ = 0;

        friend bool operator==(const bytes_cnt& lhs,
                               const bytes_cnt& rhs) noexcept
        {
            return (lhs.org_recv_bytes_ == rhs.org_recv_bytes_) &&
                   (lhs.cln_recv_bytes_ == rhs.cln_recv_bytes_);
        }
    };

    // We switch the origin between two types of streams.
    using origin_stream_t =
        detail::switchable_read_stream<tcp_socket_t, async_read_stream>;

    tcp_socket_t client_sock_;
    origin_stream_t origin_stream_;

    xutils::io_buff client_rbuf_; // client receive buffer
    xutils::io_buff origin_rbuf_; // origin receive buffer

    xutils::io_buff_reader client_rbuf_rdr_;
    xutils::io_buff_reader origin_rbuf_rdr_;

    proto_handler_ptr_t proto_handler_;

    // Bytes waiting to be send to the origin.
    // Includes the currently send bytes to the origin.
    bytes32_t origin_pending_bytes_ = 0;
    // Bytes waiting to be send to the client.
    // Includes the currently send bytes to the client.
    bytes32_t client_pending_bytes_ = 0;

    x3me::utils::pimpl<pcsm::sm, 32, 8> sm_; // The state machine

    all_stats& all_stats_;

    bytes_cnt traffic_cnt_half_closed_;
    bytes_cnt traffic_cnt_;
    bytes_cnt ref_traffic_cnt_;

    hook_t hook_half_closed_;

    const id_tag tag_;

    // Temporary added to trace a bug.
    const uint32_t thread_id_;

    const net_thread_id_t net_thread_id_;

    using sm_flags_t = uint8_t;
    enum sm_flags : sm_flags_t
    {
        // Needed only for the DSCP/TOS mark. Ugly, eh ...
        org_strm2_data_prev_time = 1 << 0,
        org_strm2_data_this_time = 1 << 1,
    };
    sm_flags_t sm_flags_ = 0;

    friend proxy_conn_ptr_t make_proxy_conn(const id_tag&,
                                            tcp_socket_t&&,
                                            bytes32_t,
                                            bytes32_t,
                                            all_stats&,
                                            uint16_t) noexcept;

public:
    using half_closed_hook_t =
        boost::intrusive::member_hook<proxy_conn,
                                      hook_t,
                                      &proxy_conn::hook_half_closed_>;
    using half_closed_t =
        boost::intrusive::list<proxy_conn,
                               half_closed_hook_t,
                               boost::intrusive::constant_time_size<true>>;
    static std::vector<half_closed_t> half_closed;

    // Set once on proxy initialization
    static x3me_sockopt::type_of_service tos_mark_hit;
    static x3me_sockopt::type_of_service tos_mark_miss;

private:
    proxy_conn(const id_tag& tag,
               tcp_socket_t&& client_sock,
               bytes32_t client_rbuf_block_size,
               bytes32_t origin_rbuf_block_size,
               all_stats& sts,
               net_thread_id_t net_tid) noexcept;

public:
    ~proxy_conn() noexcept;

    proxy_conn(const proxy_conn&) = delete;
    proxy_conn& operator=(const proxy_conn&) = delete;
    proxy_conn(proxy_conn&&) = delete;
    proxy_conn& operator=(proxy_conn&&) = delete;

    void start(const handler_factory_t& hfactory) noexcept;

    proxy_conn_ptr_t shared_from_this() noexcept;

    void register_client_reader(xutils::io_buff_reader& rdr) noexcept;
    void register_origin_reader(xutils::io_buff_reader& rdr) noexcept;
    void unregister_client_reader(xutils::io_buff_reader& rdr) noexcept;
    void unregister_origin_reader(xutils::io_buff_reader& rdr) noexcept;

    // No-op if the buffer is already this size or bigger.
    // Returns the previous buffer size.
    bytes32_t expand_client_recv_buff(bytes32_t capacity) noexcept;
    bytes32_t expand_origin_recv_buff(bytes32_t capacity) noexcept;

    // Pauses the receiving from the origin side.
    // A paused receiving can only be resumed through resume_origin_recv.
    void pause_origin_recv() noexcept;
    void resume_origin_recv() noexcept;

    // These two operations start receiving from the given side if there is
    // enough buffer space and the receiving is not currently in progress.
    void wake_up_client_recv() noexcept;
    void wake_up_origin_recv() noexcept;

    void send_to_client(bytes32_t bytes) noexcept;
    void send_to_origin(bytes32_t bytes) noexcept;

    // Executed when all pending/unconsumed bytes are sent.
    void enqueue_shutdown_client_send() noexcept;
    void enqueue_shutdown_origin_send() noexcept;
    // Executed when all pending/unconsumed bytes are sent.
    void enqueue_close_client() noexcept;
    void enqueue_close_origin() noexcept;

    // Precondition - all external readers must be freed/unregistered
    // before calling this function.
    void start_blind_tunnel() noexcept;

    bool close_if_stalled() noexcept;

    void switch_org_stream(async_read_stream&& strm) noexcept;

private:
    bool setup_origin_socket(err_code_t& err) noexcept;
    void start_org_connect() noexcept;

    void start_cln_recv() noexcept;
    void start_org_recv() noexcept;

    void start_cln_send() noexcept;
    void start_org_send() noexcept;

    void shutdown_cln_send() noexcept;
    void shutdown_org_send() noexcept;

    void close_cln() noexcept;
    void close_org() noexcept;

    void mark_half_closed() noexcept;

    void update_org_recv_bytes_stats(bytes32_t bytes) noexcept;
    void update_cln_recv_bytes_stats(bytes32_t bytes) noexcept;
    void update_org_send_bytes_stats(bytes32_t bytes) noexcept;
    void update_cln_send_bytes_stats(bytes32_t bytes) noexcept;

    void set_client_sock_tos(const x3me_sockopt::type_of_service tos) noexcept;
    void set_client_sock_tos() noexcept;

    bool in_blind_tunnel() const noexcept;
};

////////////////////////////////////////////////////////////////////////////////

inline proxy_conn_ptr_t make_proxy_conn(const id_tag& tag,
                                        tcp_socket_t&& client_sock,
                                        bytes32_t client_rbuf_block_size,
                                        bytes32_t origin_rbuf_block_size,
                                        all_stats& sts,
                                        net_thread_id_t net_tid) noexcept
{
    return new (std::nothrow)
        proxy_conn(tag, std::move(client_sock), client_rbuf_block_size,
                   origin_rbuf_block_size, sts, net_tid);
}
} // namespace net
