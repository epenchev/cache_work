#include "precompiled.h"
#include "proxy_conn.h"
#include "net_stats.h"
#include "proto_handler.h"
#include "switchable_read_stream.ipp"
#include "xutils/moveable_handler.h"

namespace net
{
namespace
{

bytes32_t fill_wr_buffer(xutils::io_buff& iob, vec_wr_buffer_t& vbuf) noexcept
{
    bytes32_t ret = 0;
    for (auto block : iob) // It's intentionally by value
    {
        ret += block.size();
        vbuf.push_back(boost::asio::buffer(block.data(), block.size()));
    }
    return ret;
}

bytes32_t fill_vec_ro_buffer(const xutils::io_buff_reader& ior,
                             bytes32_t to_fill,
                             vec_ro_buffer_t& vbuf) noexcept
{
    bytes32_t ret = 0;
    for (auto it = ior.begin(); (it != ior.end()) && (to_fill > 0); ++it)
    {
        const auto block = *it;
        const auto sz = std::min<bytes32_t>(to_fill, block.size());
        vbuf.push_back(boost::asio::buffer(block.data(), sz));
        ret += sz;
        to_fill -= sz;
    }
    assert(to_fill == 0);
    return ret;
}

void set_keep_alive_sock_opts(tcp_socket_t& sock, const id_tag& tag) noexcept
{
    try
    {
        using boost::asio::socket_base;
        sock.set_option(socket_base::keep_alive(true));
        sock.set_option(x3me_sockopt::tcp_keep_idle(300));
        sock.set_option(x3me_sockopt::tcp_keep_cnt(5));
    }
    catch (const bsys::system_error& err)
    {
        XLOG_ERROR(tag, "Proxy_conn. Failed to set socket "
                        "keep_alive options. {}",
                   err.what());
    }
}

} // namespace
////////////////////////////////////////////////////////////////////////////////
namespace pcsm // Proxy Connection State Machine
{

// clang-format off
struct ev_org_connect {};
struct ev_org_connected {};
struct ev_org_recv {};
struct ev_org_recv_data {};
struct ev_org_recv_eof {};
struct ev_org_recv_err {};
struct ev_org_send {};
struct ev_org_send_shut {};
struct ev_org_sent_data {};
struct ev_org_sent_err {};
struct ev_org_close{};
struct ev_cln_recv {};
struct ev_cln_recv_data {};
struct ev_cln_recv_eof {};
struct ev_cln_recv_err {};
struct ev_cln_send {};
struct ev_cln_send_shut {};
struct ev_cln_sent_data {};
struct ev_cln_sent_err {};
struct ev_cln_close {};
struct ev_org_recv_pause {};
struct ev_org_recv_resume {};
// clang-format on

struct sm_impl
{
    struct on_unexpected_event
    {
        template <typename Ev>
        void operator()(proxy_conn* inst, const Ev& ev) noexcept;
    };

    auto operator()() noexcept
    {
        // clang-format off
        auto no_act = []{};

        auto org_recv_allowed = [](proxy_conn* c)
        {
            // Don't issue too small reads and
            // do read only if the other leg is alive.
            return (c->origin_rbuf_.bytes_avail_wr() >= 
                    (c->origin_rbuf_.block_size() / 2)) &&
                    c->client_sock_.is_open();
        };
        auto start_org_recv = [](proxy_conn* c){ c->start_org_recv(); };

        auto org_send_allowed = [](proxy_conn* c)
        {
            return (c->origin_pending_bytes_ > 0) && 
                    c->origin_stream_.is<tcp_socket_t>();
        };
        auto start_org_send = [](proxy_conn* c){ c->start_org_send(); };

        auto has_org_unconsumed = [](proxy_conn* c)
        {
            return (c->origin_rbuf_rdr_.bytes_avail() > 0);
        };
        auto shut_cln_send = [](proxy_conn* c){ c->shutdown_cln_send(); };
        auto close_cln = [](proxy_conn* c){ c->close_cln(); };

        auto cln_recv_allowed = [](proxy_conn* c)
        {
            // Don't issue too small reads and
            // do read only if the other leg is alive.
            return (c->client_rbuf_.bytes_avail_wr() >= 
                    (c->client_rbuf_.block_size() / 2)) &&
                    c->origin_stream_.is_open();
        };
        auto start_cln_recv = [](proxy_conn* c){ c->start_cln_recv(); };

        auto cln_send_allowed = [](proxy_conn* c)
        {
            return c->client_pending_bytes_ > 0;
        };
        auto start_cln_send = [](proxy_conn* c){ c->start_cln_send(); };

        auto need_proxy_cln_data = [](proxy_conn* c)
        {
            return (c->client_rbuf_rdr_.bytes_avail() > 0) &&
                c->origin_stream_.is<tcp_socket_t>();
        };
        auto shut_org_send = [](proxy_conn* c){ c->shutdown_org_send(); };
        auto close_org = [](proxy_conn* c){ c->close_org(); };

        // The state machine ensures that we start send/recv operations only
        // when appropriate i.e. we don't start recv when there is already one
        // in progress, or when there is no buffer space for writing, etc.
        // The state machine doesn't have separate states for the blind tunnel
        // mode, because this would lead increase the transition table
        // almost two times. Thus we have ifs in 2-3 places, for the tunnel
        // mode, which is, a ... compromise :).
        using namespace boost::sml;
        const auto org_recv_start_s  = "org_recv_start"_s;
        const auto org_recv_conn_s   = "org_recv_conn"_s;
        const auto org_recv_idle_s   = "org_recv_idle"_s;
        const auto org_recv_s        = "org_recv"_s;
        const auto org_recv_eof_s    = "org_recv_eof"_s;
        const auto org_recv_err_s    = "org_recv_err"_s;
        const auto org_recv_paused_s = "org_recv_paused"_s;
        const auto org_send_start_s  = "org_send_start"_s;
        const auto org_send_conn_s   = "org_send_conn"_s;
        const auto org_send_idle_s   = "org_send_idle"_s;
        const auto org_send_err_s    = "org_send_err"_s;
        const auto org_send_s        = "org_send"_s;
        const auto org_send_shut_s   = "org_send_shut"_s;
        const auto org_pend_shut_s   = "org_pend_shut"_s;
        const auto org_wait_end_s    = "org_wait_end"_s;
        const auto org_pend_close_s  = "org_pend_close"_s;
        const auto org_closed_s      = "org_closed"_s;
        const auto cln_recv_idle_s   = "cln_recv_idle"_s;
        const auto cln_recv_s        = "cln_recv"_s;
        const auto cln_recv_eof_s    = "cln_recv_eof"_s;
        const auto cln_recv_err_s    = "cln_recv_err"_s;
        const auto cln_send_idle_s   = "cln_send_idle"_s;
        const auto cln_send_err_s    = "cln_send_err"_s;
        const auto cln_send_s        = "cln_send"_s;
        const auto cln_send_shut_s   = "cln_send_shut"_s;
        const auto cln_pend_shut_s   = "cln_pend_shut"_s;
        const auto cln_wait_end_s    = "cln_wait_end"_s;
        const auto cln_pend_close_s  = "cln_pend_close"_s;
        const auto cln_closed_s      = "cln_closed"_s;
        return make_transition_table(
            // Origin receiving
            *org_recv_start_s + event<ev_org_connect> = org_recv_conn_s,
            org_recv_conn_s + event<ev_org_connected> = org_recv_idle_s,
            org_recv_idle_s + event<ev_org_recv> [org_recv_allowed] /
                                                start_org_recv = org_recv_s,
            org_recv_idle_s + event<ev_org_recv> [!org_recv_allowed],
            // Used after the switched stream gives EOF
            org_recv_idle_s + event<ev_org_recv_eof> = org_recv_eof_s,
            org_recv_idle_s + event<ev_org_recv_pause> = org_recv_paused_s,
            org_recv_paused_s + event<ev_org_recv_resume> [!org_recv_allowed] 
                                                            = org_recv_idle_s,
            org_recv_paused_s + event<ev_org_recv_resume> [org_recv_allowed] /
                                                start_org_recv = org_recv_s,
            org_recv_s + event<ev_org_recv> / no_act,
            org_recv_s + event<ev_org_recv_data> = org_recv_idle_s,
            org_recv_s + event<ev_org_recv_eof> = org_recv_eof_s,
            org_recv_s + event<ev_org_recv_err> = org_recv_err_s,
            org_recv_eof_s + event<ev_org_recv> / no_act,
            org_recv_err_s + event<ev_org_recv> / no_act,
            org_recv_paused_s + event<ev_org_recv> / no_act,
            // Origin sending
            *org_send_start_s + event<ev_org_connect> = org_send_conn_s,
            org_send_conn_s + event<ev_org_connected> = org_send_idle_s,
            org_send_idle_s + event<ev_org_send> [org_send_allowed] / 
                                                start_org_send = org_send_s,
            org_send_idle_s + event<ev_org_send> [!org_send_allowed],
            org_send_s + event<ev_org_send> / no_act,
            org_send_s + event<ev_org_sent_data> = org_send_idle_s,
            org_send_s + event<ev_org_sent_err> = org_send_err_s,
            org_send_err_s + event<ev_org_send> / no_act,
            // Client receiving
            *cln_recv_idle_s + event<ev_cln_recv> [cln_recv_allowed] /
                                                start_cln_recv = cln_recv_s,
            cln_recv_idle_s + event<ev_cln_recv> [!cln_recv_allowed],
            cln_recv_s + event<ev_cln_recv> / no_act,
            cln_recv_s + event<ev_cln_recv_data> = cln_recv_idle_s,
            cln_recv_s + event<ev_cln_recv_eof> = cln_recv_eof_s,
            cln_recv_s + event<ev_cln_recv_err> = cln_recv_err_s,
            cln_recv_eof_s + event<ev_cln_recv> / no_act,
            cln_recv_err_s + event<ev_cln_recv> / no_act,
            // Client sending
            *cln_send_idle_s + event<ev_cln_send> [cln_send_allowed] / 
                                                start_cln_send = cln_send_s,
            cln_send_idle_s + event<ev_cln_send> [!cln_send_allowed],
            cln_send_s + event<ev_cln_send> / no_act,
            cln_send_s + event<ev_cln_sent_data> = cln_send_idle_s,
            cln_send_s + event<ev_cln_sent_err> = cln_send_err_s,
            cln_send_err_s + event<ev_cln_send> / no_act,
            // Origin shutdown/close
            *org_wait_end_s + event<ev_org_send_shut> [need_proxy_cln_data] =
                                                        org_pend_shut_s,
            org_wait_end_s + event<ev_org_send_shut> [!need_proxy_cln_data] /
                                            shut_org_send = org_send_shut_s,
            org_pend_shut_s + event<ev_org_sent_data> [!need_proxy_cln_data]
                                            / shut_org_send = org_send_shut_s,
            // If we have pending shutdown we must have unconsumed data,
            // thus we can't directly close the socket.
            org_wait_end_s + event<ev_org_close> [need_proxy_cln_data] =
                                                       org_pend_close_s,
            org_wait_end_s + event<ev_org_close> [!need_proxy_cln_data] /
                                                close_org = org_closed_s,
            org_pend_shut_s + event<ev_org_close> = org_pend_close_s,
            org_pend_close_s + event<ev_org_sent_data> [!need_proxy_cln_data]
                                            / close_org = org_closed_s,
            org_pend_close_s + event<ev_org_send_shut> / no_act,
            org_pend_close_s + event<ev_org_close> / no_act,
            org_send_shut_s + event<ev_org_close> / close_org = org_closed_s,
            org_closed_s + event<ev_org_send_shut> / no_act,
            org_closed_s + event<ev_org_close> / no_act,
            // Client shutdown/close
            *cln_wait_end_s + event<ev_cln_send_shut> [has_org_unconsumed] =
                                                        cln_pend_shut_s,
            cln_wait_end_s + event<ev_cln_send_shut> [!has_org_unconsumed] /
                                        shut_cln_send = cln_send_shut_s,
            cln_pend_shut_s + event<ev_cln_sent_data> [!has_org_unconsumed]
                                        / shut_cln_send = cln_send_shut_s,
            // If we have pending shutdown we must have unconsumed data,
            // thus we can't directly close the socket.
            cln_wait_end_s + event<ev_cln_close> [has_org_unconsumed] =
                                                        cln_pend_close_s,
            cln_wait_end_s + event<ev_cln_close> [!has_org_unconsumed] /
                                                close_cln = cln_closed_s,
            cln_pend_shut_s + event<ev_cln_close> = cln_pend_close_s,
            cln_pend_close_s + event<ev_cln_sent_data> [!has_org_unconsumed]
                                            / close_cln = cln_closed_s,
            cln_pend_close_s + event<ev_cln_send_shut> / no_act,
            cln_pend_close_s + event<ev_cln_close> / no_act,
            cln_send_shut_s + event<ev_cln_close> / close_cln = cln_closed_s,
            cln_closed_s + event<ev_cln_send_shut> / no_act,
            cln_closed_s + event<ev_cln_close> / no_act
            );
        // clang-format on
    }
};

template <typename E>
auto event_type() // Gives the event type string
{
    return __PRETTY_FUNCTION__;
}

class sm : private boost::sml::sm<sm_impl>
{
    using base_t = boost::sml::sm<sm_impl>;

public:
    using base_t::base_t;

    template <typename Ev>
    void process_event(Ev ev) noexcept
    {
        const bool r = base_t::process_event(ev);
        if (X3ME_UNLIKELY(!r))
        {
            std::cerr << "BUG in Proxy_conn state machine\n";
            base_t::visit_current_states(
                [](const auto& st)
                {
                    std::cerr << "\tNo transition from state " << st.c_str()
                              << " on " << event_type<Ev>() << '\n';
                });
            std::cerr << std::flush;
            ::abort();
        }
    }

    bool is_org_strm_paused() const noexcept
    {
        using namespace boost::sml;
        return base_t::is("org_recv_paused"_s);
    }

    bool has_org_err() const noexcept
    {
        using namespace boost::sml;
        return in_any_state("org_recv_err"_s, "org_send_err"_s);
    }

    bool has_cln_err() const noexcept
    {
        using namespace boost::sml;
        return in_any_state("cln_recv_err"_s, "cln_send_err"_s);
    }

private:
    template <typename S1, typename S2>
    bool in_any_state(S1, S2) const noexcept
    {
        bool ret = false;
        base_t::visit_current_states([&](auto st)
                                     {
                                         using t1 = typename S1::type;
                                         using t2 = typename S2::type;
                                         using tt = typename decltype(st)::type;
                                         if (std::is_same<t1, tt>::value ||
                                             std::is_same<t2, tt>::value)
                                             ret = true;
                                     });
        return ret;
    }
};

} // namespace pcsm
////////////////////////////////////////////////////////////////////////////////
std::vector<proxy_conn::half_closed_t> proxy_conn::half_closed;
x3me_sockopt::type_of_service proxy_conn::tos_mark_hit;
x3me_sockopt::type_of_service proxy_conn::tos_mark_miss;

proxy_conn::proxy_conn(const id_tag& tag,
                       tcp_socket_t&& client_sock,
                       bytes32_t client_rbuf_block_size,
                       bytes32_t origin_rbuf_block_size,
                       all_stats& sts,
                       net_thread_id_t net_tid) noexcept
    : client_sock_(std::move(client_sock)),
      origin_stream_(tcp_socket_t{client_sock_.get_io_service()}),
      client_rbuf_(client_rbuf_block_size),
      origin_rbuf_(origin_rbuf_block_size),
      sm_(this),
      all_stats_(sts),
      // Make sure that we don't close half closed connection which
      // hasn't been checked yet and hasn't received anything.
      ref_traffic_cnt_{(bytes32_t)-1, (bytes32_t)-1},
      tag_(tag),
      thread_id_(x3me::sys_utils::thread_id()),
      net_thread_id_(net_tid)
{
    XLOG_DEBUG(tag_, "Proxy_conn created. Net_thread_id {}. "
                     "Client_rbuf_block_size {} bytes. "
                     "Origin_rbuf_block_size {} bytes",
               net_tid, client_rbuf_block_size, origin_rbuf_block_size);
    ++all_stats_.cnt_curr_conns_;
}

proxy_conn::~proxy_conn() noexcept
{
    if (X3ME_UNLIKELY(thread_id_ != x3me::sys_utils::thread_id()))
    {
        char name[24] = {};
        x3me::sys_utils::get_this_thread_name(name, sizeof(name));
        std::cerr << "BUG!!! Proxy connection destructed in a non-owner thread "
                  << name << std::endl;
        XLOG_FATAL(tag_, "Proxy_conn destructed in a non-owner thread {}",
                   name);
    }
    XLOG_INFO(tag_, "Proxy_conn destroyed");
    if (hook_half_closed_.is_linked())
    {
        half_closed[net_thread_id_].erase(half_closed_t::s_iterator_to(*this));
        all_stats_.cnt_half_closed_ += 1;
    }
    if (traffic_cnt_half_closed_.cln_recv_bytes_ > 0)
    {
        all_stats_.cnt_half_closed_cln_recv_ += 1;
        // Add one if the connection to the origin has been, in fact,
        // fully closed and we couldn't send any bytes
        all_stats_.cnt_half_closed_org_closed_ +=
            (client_rbuf_rdr_.bytes_avail() >=
             traffic_cnt_half_closed_.cln_recv_bytes_);
    }
    if (traffic_cnt_half_closed_.org_recv_bytes_ > 0)
    {
        all_stats_.cnt_half_closed_org_recv_ += 1;
        // Add one if the connection to the client has been, in fact,
        // fully closed and we couldn't send any bytes
        all_stats_.cnt_half_closed_cln_closed_ +=
            (origin_rbuf_rdr_.bytes_avail() >=
             traffic_cnt_half_closed_.org_recv_bytes_);
    }
    all_stats_.cnt_curr_conns_ -= 1;
    all_stats_.cnt_curr_blind_tunnel_ -= in_blind_tunnel();
}

void proxy_conn::start(const handler_factory_t& hfactory) noexcept
{
    err_code_t err;
    if (setup_origin_socket(err))
    {
        proto_handler_ =
            hfactory(tag_, client_sock_.get_io_service(), net_thread_id_);
        set_client_sock_tos(tos_mark_miss);
        start_org_connect();
    }
    else
    {
        XLOG_WARN(tag_,
                  "Proxy_conn start failed. Unable to setup origin socket. {}",
                  err.message());
        // The connection remains without protocol handler and in order to
        // keep correct statistics we need to increase the counter for the
        // blind tunnel connections (see proxy_conn destructor above).
        ++all_stats_.cnt_curr_blind_tunnel_;
    }
}

proxy_conn_ptr_t proxy_conn::shared_from_this() noexcept
{
    return this;
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::register_client_reader(xutils::io_buff_reader& rdr) noexcept
{
    XLOG_TRACE(tag_, "Proxy_conn::register_client_reader {}", (void*)&rdr);
    client_rbuf_.register_reader(rdr);
}

void proxy_conn::register_origin_reader(xutils::io_buff_reader& rdr) noexcept
{
    XLOG_TRACE(tag_, "Proxy_conn::register_origin_reader {}", (void*)&rdr);
    origin_rbuf_.register_reader(rdr);
}

void proxy_conn::unregister_client_reader(xutils::io_buff_reader& rdr) noexcept
{
    XLOG_TRACE(tag_, "Proxy_conn::unregister_client_reader {}", (void*)&rdr);
    client_rbuf_.unregister_reader(rdr);
}

void proxy_conn::unregister_origin_reader(xutils::io_buff_reader& rdr) noexcept
{
    XLOG_TRACE(tag_, "Proxy_conn::unregister_origin_reader {}", (void*)&rdr);
    origin_rbuf_.unregister_reader(rdr);
}

bytes32_t proxy_conn::expand_client_recv_buff(bytes32_t capacity) noexcept
{
    const auto old_capacity = client_rbuf_.capacity();
    if (capacity > old_capacity)
    {
        XLOG_DEBUG(tag_, "Proxy_conn::expand_client_recv_buff to {} bytes",
                   capacity);
        client_rbuf_.expand_with(capacity - old_capacity);
    }
    return old_capacity;
}

bytes32_t proxy_conn::expand_origin_recv_buff(bytes32_t capacity) noexcept
{
    const auto old_capacity = origin_rbuf_.capacity();
    if (capacity > old_capacity)
    {
        XLOG_DEBUG(tag_, "Proxy_conn::expand_origin_recv_buff to {} bytes",
                   capacity);
        origin_rbuf_.expand_with(capacity - old_capacity);
    }
    return old_capacity;
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::pause_origin_recv() noexcept
{
    sm_->process_event(pcsm::ev_org_recv_pause{});
}

void proxy_conn::resume_origin_recv() noexcept
{
    sm_->process_event(pcsm::ev_org_recv_resume{});
}

void proxy_conn::wake_up_client_recv() noexcept
{
    sm_->process_event(pcsm::ev_cln_recv{});
}

void proxy_conn::wake_up_origin_recv() noexcept
{
    sm_->process_event(pcsm::ev_org_recv{});
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::send_to_client(bytes32_t bytes) noexcept
{
    const auto rdr_avail = origin_rbuf_rdr_.bytes_avail();
    client_pending_bytes_ += bytes;
    assert(client_pending_bytes_ <= rdr_avail);
    XLOG_DEBUG(tag_,
               "Proxy_conn::send_to_client {} bytes. All_pending_bytes {}. "
               "Origin_reader_bytes {}",
               bytes, client_pending_bytes_, rdr_avail);
    sm_->process_event(pcsm::ev_cln_send{});
}

void proxy_conn::send_to_origin(bytes32_t bytes) noexcept
{
    const auto rdr_avail = client_rbuf_rdr_.bytes_avail();
    origin_pending_bytes_ += bytes;
    assert(origin_pending_bytes_ <= rdr_avail);
    XLOG_DEBUG(tag_,
               "Proxy_conn::send_to_origin {} bytes. All_pending_bytes {}. "
               "Client_reader_bytes {}",
               bytes, origin_pending_bytes_, rdr_avail);
    sm_->process_event(pcsm::ev_org_send{});
}

void proxy_conn::enqueue_shutdown_client_send() noexcept
{
    sm_->process_event(pcsm::ev_cln_send_shut{});
}

void proxy_conn::enqueue_shutdown_origin_send() noexcept
{
    sm_->process_event(pcsm::ev_org_send_shut{});
}

void proxy_conn::enqueue_close_client() noexcept
{
    sm_->process_event(pcsm::ev_cln_close{});
}

void proxy_conn::enqueue_close_origin() noexcept
{
    sm_->process_event(pcsm::ev_org_close{});
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::start_blind_tunnel() noexcept
{
    const auto cln_bytes_avail = client_rbuf_rdr_.bytes_avail();
    const auto org_bytes_avail = origin_rbuf_rdr_.bytes_avail();

    assert(proto_handler_);
    // All bytes must be available only for us reading/writing otherwise we
    // won't be able do find out when the upper layer readers are silently
    // unregistered later.
    assert((cln_bytes_avail + client_rbuf_.bytes_avail_wr() + 1) ==
           client_rbuf_.capacity());
    assert((org_bytes_avail + origin_rbuf_.bytes_avail_wr() + 1) ==
           origin_rbuf_.capacity());

    origin_pending_bytes_ = cln_bytes_avail;
    client_pending_bytes_ = org_bytes_avail;
    XLOG_INFO(tag_, "Proxy_conn. Starting blind tunnel. "
                    "Origin_pending_bytes {}. Client_pending_bytes {}",
              origin_pending_bytes_, client_pending_bytes_);
    ++all_stats_.cnt_curr_blind_tunnel_;
    client_sock_.get_io_service().post(
        xutils::make_moveable_handler([ph = std::move(proto_handler_)]() mutable
                                      {
                                          // Explicit, for clarity
                                          ph.reset();
                                      }));
    if (!sm_->has_org_err())
    {
        // Auto resume paused connection if blind tunnel is requested
        if (sm_->is_org_strm_paused())
            sm_->process_event(pcsm::ev_org_recv_resume{});
        sm_->process_event(pcsm::ev_org_recv{});
        sm_->process_event(pcsm::ev_org_send{});
    }
    else
    { // Close the client connection when the pending data are flushed
        sm_->process_event(pcsm::ev_cln_close{});
    }
    if (!sm_->has_cln_err())
    {
        sm_->process_event(pcsm::ev_cln_recv{});
        sm_->process_event(pcsm::ev_cln_send{});
    }
    else
    { // Close the origin connection when the pending data are flushed
        sm_->process_event(pcsm::ev_org_close{});
    }
}

bool proxy_conn::close_if_stalled() noexcept
{
    const bool res = (traffic_cnt_ == ref_traffic_cnt_);
    if (res)
    {
        close_cln();
        close_org();
    }
    else
    {
        ref_traffic_cnt_ = traffic_cnt_;
    }
    return res;
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::switch_org_stream(async_read_stream&& strm) noexcept
{
    X3ME_ASSERT(sm_->is_org_strm_paused(), "This method must be called only "
                                           "when the current origin stream is "
                                           "stream1 and it's paused");
    XLOG_INFO(tag_, "Switching to org_async_read_stream");
    origin_stream_ = std::move(strm);
}

////////////////////////////////////////////////////////////////////////////////

bool proxy_conn::setup_origin_socket(err_code_t& err) noexcept
{
    try
    {
        using namespace boost::asio;

        const auto cep     = tag_.user_endpoint();
        const auto bind_ep = tcp_endpoint_t{cep.address(), cep.port()};

        auto& sock = origin_stream_.get<tcp_socket_t>();

        sock.open(ip::tcp::v4());
        sock.set_option(x3me_sockopt::transparent_mode(true));
        sock.set_option(socket_base::reuse_address(true));
        sock.bind(bind_ep);
    }
    catch (const boost::system::system_error& e)
    {
        err = e.code();
        return false;
    }
    return true;
}

void proxy_conn::start_org_connect() noexcept
{
    XLOG_INFO(tag_, "Proxy_conn start connecting to the origin");

    const auto oep = tag_.server_endpoint();
    const tcp_endpoint_t origin_ep{oep.address(), oep.port()};

    proto_handler_->on_origin_pre_connect(*this);

    sm_->process_event(pcsm::ev_org_connect{});

    ++all_stats_.cnt_curr_connecting_;
    auto& sock = origin_stream_.get<tcp_socket_t>();
    sock.async_connect(
        origin_ep, [inst = shared_from_this()](const err_code_t& err)
        {
            auto& sts = inst->all_stats_;
            --sts.cnt_curr_connecting_;
            if (!err)
            {
                XLOG_INFO(inst->tag_, "Proxy_conn connected to the origin");
                ++sts.cnt_connect_success_;

                auto& origin_sock = inst->origin_stream_.get<tcp_socket_t>();
                set_keep_alive_sock_opts(origin_sock, inst->tag_);
                set_keep_alive_sock_opts(inst->client_sock_, inst->tag_);

                auto* sm = inst->sm_.get();
                sm->process_event(pcsm::ev_org_connected{});

                inst->client_rbuf_.register_reader(inst->client_rbuf_rdr_);
                inst->origin_rbuf_.register_reader(inst->origin_rbuf_rdr_);

                inst->proto_handler_->init(*inst);

                sm->process_event(pcsm::ev_cln_recv{});
                sm->process_event(pcsm::ev_org_recv{});
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_INFO(inst->tag_,
                          "Proxy_conn failed connect to the origin. {}",
                          err.message());
                ++sts.cnt_connect_failed_;
            }
        });
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::start_cln_recv() noexcept
{
    vec_wr_buffer_t buff;
    const auto bytes_recv = fill_wr_buffer(client_rbuf_, buff);
    XLOG_DEBUG(tag_, "Proxy_conn. Try receive {} bytes from client",
               bytes_recv);
    client_sock_.async_read_some(
        buff,
        [inst = shared_from_this()](const err_code_t& err, bytes32_t bytes)
        {
            auto* sm = inst->sm_.get();
            if (!err)
            {
                XLOG_DEBUG(inst->tag_,
                           "Proxy_conn. Received {} bytes from client", bytes);
                inst->update_cln_recv_bytes_stats(bytes);

                inst->client_rbuf_.commit(bytes);
                sm->process_event(pcsm::ev_cln_recv_data{});

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_client_data(*inst);
                else
                {
                    inst->origin_pending_bytes_ =
                        inst->client_rbuf_rdr_.bytes_avail();
                    sm->process_event(pcsm::ev_org_send{});
                }
                sm->process_event(pcsm::ev_cln_recv{});
            }
            else if (err == asio_error::eof)
            {
                XLOG_INFO(inst->tag_, "Proxy_conn. EOF in client receiving");
                sm->process_event(pcsm::ev_cln_recv_eof{});

                inst->mark_half_closed();

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_client_recv_eof(*inst);
                else
                    sm->process_event(pcsm::ev_org_send_shut{});
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_INFO(inst->tag_,
                          "Proxy_conn. Error in client receiving. {}",
                          err.message());
                sm->process_event(pcsm::ev_cln_recv_err{});

                inst->close_cln(); // Free OS resource
                inst->mark_half_closed();

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_client_recv_err(*inst);
                else
                    sm->process_event(pcsm::ev_org_close{});
            }
        });
}

void proxy_conn::start_org_recv() noexcept
{
    vec_wr_buffer_t buff;
    const auto bytes_recv = fill_wr_buffer(origin_rbuf_, buff);
    XLOG_DEBUG(tag_, "Proxy_conn. Try receive {} bytes from origin",
               bytes_recv);
    origin_stream_.async_read_some(
        buff,
        [inst = shared_from_this()](const err_code_t& err, bytes32_t bytes)
        {
            auto* sm           = inst->sm_.get();
            const bool s2_data = inst->origin_stream_.is<async_read_stream>();
            if (bytes > 0)
            {
                XLOG_DEBUG(
                    inst->tag_,
                    "Proxy_conn. Received {} bytes from origin. From_strm2 {}",
                    bytes, s2_data);
                inst->update_org_recv_bytes_stats(bytes);
                // Needed for the send to client so that it can set
                // correct DSCP/TOS mark.
                if (s2_data)
                    inst->sm_flags_ |= sm_flags::org_strm2_data_this_time;
                else
                    inst->sm_flags_ &= ~sm_flags::org_strm2_data_this_time;

                inst->origin_rbuf_.commit(bytes);
                sm->process_event(pcsm::ev_org_recv_data{});

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_origin_data(*inst);
                else
                {
                    inst->client_pending_bytes_ =
                        inst->origin_rbuf_rdr_.bytes_avail();
                    sm->process_event(pcsm::ev_cln_send{});
                }
            }
            if (!err)
            {
                sm->process_event(pcsm::ev_org_recv{});
            }
            else if (err == asio_error::eof)
            {
                XLOG_INFO(inst->tag_,
                          "Proxy_conn. EOF in origin receiving. From_strm2 {}",
                          s2_data);
                sm->process_event(pcsm::ev_org_recv_eof{});
                inst->mark_half_closed();
                // This is a bit of a hack, but we need to act in different
                // ways depending on the type of the current origin stream.
                if (s2_data)
                {
                    inst->close_org(); // Free OS resource
                    if (!inst->in_blind_tunnel())
                        inst->proto_handler_->on_switched_stream_eof(*inst);
                    else
                        sm->process_event(pcsm::ev_cln_close{});
                }
                else
                {
                    if (!inst->in_blind_tunnel())
                        inst->proto_handler_->on_origin_recv_eof(*inst);
                    else
                        sm->process_event(pcsm::ev_cln_send_shut{});
                }
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_INFO(
                    inst->tag_,
                    "Proxy_conn. Error in origin receiving. From_strm2 {}. {}",
                    s2_data, err.message());
                sm->process_event(pcsm::ev_org_recv_err{});

                inst->close_org(); // Free OS resource
                inst->mark_half_closed();

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_origin_recv_err(*inst);
                else
                    sm->process_event(pcsm::ev_cln_close{});
            }
        });
}

void proxy_conn::start_cln_send() noexcept
{
    using namespace boost;
    vec_ro_buffer_t buff;
    const auto bytes_to_send =
        fill_vec_ro_buffer(origin_rbuf_rdr_, client_pending_bytes_, buff);
    XLOG_DEBUG(tag_, "Proxy_conn. Sending {} bytes to client", bytes_to_send);
    set_client_sock_tos();
    asio::async_write(
        client_sock_, buff, asio::transfer_all(),
        [inst = shared_from_this()](const err_code_t& err, bytes32_t bytes)
        {
            auto* sm = inst->sm_.get();
            if (!err)
            {
                XLOG_DEBUG(inst->tag_, "Proxy_conn. Sent {} bytes to client",
                           bytes);
                X3ME_ASSERT(inst->client_pending_bytes_ >= bytes);
                inst->client_pending_bytes_ -= bytes;
                inst->origin_rbuf_rdr_.consume(bytes);
                inst->update_cln_send_bytes_stats(bytes);

                sm->process_event(pcsm::ev_cln_sent_data{});
                sm->process_event(pcsm::ev_cln_send{});
                sm->process_event(pcsm::ev_org_recv{});
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_INFO(inst->tag_, "Proxy_conn. Error in client send. {}",
                          err.message());
                sm->process_event(pcsm::ev_cln_sent_err{});

                inst->close_cln(); // Free OS resource
                inst->mark_half_closed();

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_client_send_err(*inst);
                else
                    sm->process_event(pcsm::ev_org_close{});
            }
        });
}

void proxy_conn::start_org_send() noexcept
{
    using namespace boost;
    vec_ro_buffer_t buff;
    const auto bytes_to_send =
        fill_vec_ro_buffer(client_rbuf_rdr_, origin_pending_bytes_, buff);
    XLOG_DEBUG(tag_, "Proxy_conn. Sending {} bytes to origin", bytes_to_send);
    auto& sock = origin_stream_.get<tcp_socket_t>();
    asio::async_write(
        sock, buff, asio::transfer_all(),
        [inst = shared_from_this()](const err_code_t& err, bytes32_t bytes)
        {
            auto* sm = inst->sm_.get();
            if (!err)
            {
                XLOG_DEBUG(inst->tag_, "Proxy_conn. Sent {} bytes to origin",
                           bytes);
                X3ME_ASSERT(inst->origin_pending_bytes_ >= bytes);
                inst->origin_pending_bytes_ -= bytes;
                inst->client_rbuf_rdr_.consume(bytes);
                inst->update_org_send_bytes_stats(bytes);

                sm->process_event(pcsm::ev_org_sent_data{});
                sm->process_event(pcsm::ev_org_send{});
                sm->process_event(pcsm::ev_cln_recv{});
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_INFO(inst->tag_, "Proxy_conn. Error in origin send. {}",
                          err.message());
                sm->process_event(pcsm::ev_org_sent_err{});

                inst->close_org(); // Free OS resource
                inst->mark_half_closed();

                if (!inst->in_blind_tunnel())
                    inst->proto_handler_->on_origin_send_err(*inst);
                else
                    sm->process_event(pcsm::ev_cln_close{});
            }
        });
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::shutdown_cln_send() noexcept
{
    XLOG_INFO(tag_, "Shutdown client send");
    err_code_t ignore;
    client_sock_.shutdown(asio_shutdown_t::shutdown_send, ignore);
}

void proxy_conn::shutdown_org_send() noexcept
{
    XLOG_INFO(tag_, "Shutdown origin send");
    err_code_t ignore;
    // It's no-op for the cases when the origin sock is closed/still not open.
    origin_stream_.shutdown(asio_shutdown_t::shutdown_send, ignore);
}

void proxy_conn::close_cln() noexcept
{
    XLOG_INFO(tag_, "Close client");
    err_code_t ignore;
    client_sock_.close(ignore);
}

void proxy_conn::close_org() noexcept
{
    XLOG_INFO(tag_, "Close origin");
    err_code_t ignore;
    origin_stream_.close(ignore);
}

void proxy_conn::mark_half_closed() noexcept
{
    // It's possible that we are already hooked if we have received EOF/error
    // from one of the legs and later receive EOF/error from the other one.
    if (!hook_half_closed_.is_linked())
    {
        XLOG_INFO(tag_, "Mark connection as half closed");
        half_closed[net_thread_id_].push_back(*this);
    }
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::update_org_recv_bytes_stats(bytes32_t bytes) noexcept
{
    all_stats_.bytes_all_origin_recv_ += bytes;
    traffic_cnt_.org_recv_bytes_ += bytes;
    if (hook_half_closed_.is_linked() && (client_rbuf_rdr_.bytes_avail() == 0))
    { // Start counting these bytes after we have sent everything,
        // received before EOF from the client, to the origin.
        traffic_cnt_half_closed_.org_recv_bytes_ += bytes;
    }
}

void proxy_conn::update_cln_recv_bytes_stats(bytes32_t bytes) noexcept
{
    all_stats_.bytes_all_client_recv_ += bytes;
    traffic_cnt_.cln_recv_bytes_ += bytes;
    if (hook_half_closed_.is_linked() && (origin_rbuf_rdr_.bytes_avail() == 0))
    { // Start counting these bytes after we have sent everything,
        // received before EOF from the origin, to the client.
        traffic_cnt_half_closed_.cln_recv_bytes_ += bytes;
    }
}

void proxy_conn::update_org_send_bytes_stats(bytes32_t bytes) noexcept
{
    all_stats_.bytes_all_origin_send_ += bytes;
}

void proxy_conn::update_cln_send_bytes_stats(bytes32_t bytes) noexcept
{
    const bool bytes_from_strm2 =
        (sm_flags_ & sm_flags::org_strm2_data_prev_time);
    all_stats_.bytes_all_client_send_ += bytes;
    all_stats_.bytes_hit_client_send_ += (bytes_from_strm2 * bytes);
}

////////////////////////////////////////////////////////////////////////////////

void proxy_conn::set_client_sock_tos(
    const x3me_sockopt::type_of_service tos) noexcept
{
    err_code_t err;
    client_sock_.set_option(tos, err);
    if (!err)
    {
        XLOG_DEBUG(tag_, "Set cl_socket TOS to {}", tos.value());
    }
    else
    {
        XLOG_ERROR(tag_, "Unable to set cl_socket TOS to {}. {}", tos.value(),
                   err.message());
    }
}

void proxy_conn::set_client_sock_tos() noexcept
{
    const auto data_type = (sm_flags_ & (sm_flags::org_strm2_data_this_time |
                                         sm_flags::org_strm2_data_prev_time));
    if (data_type == sm_flags::org_strm2_data_this_time)
    {
        // Previous data has been delivered from strm1, the current data
        // are delivered from strm2. We need to change socket TOS to HIT.
        set_client_sock_tos(tos_mark_hit);
        sm_flags_ |= sm_flags::org_strm2_data_prev_time;
    }
    else if (data_type == sm_flags::org_strm2_data_prev_time)
    {
        // Previous data has been delivered from strm2, the current data
        // are delivered from strm1. We need to change the socket TOS to MISS.
        set_client_sock_tos(tos_mark_miss);
        sm_flags_ &= ~sm_flags::org_strm2_data_prev_time;
    }
    // We don't need to change the mark if we don't change the delivery source
}

bool proxy_conn::in_blind_tunnel() const noexcept
{
    return !proto_handler_;
}

} // namespace net
