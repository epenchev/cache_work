#include "precompiled.h"
#include "http_handler.h"
#include "async_cache_reader.h"
#include "http_bp_ctl.h"
#include "http_constants.h"
#include "http_stats.h"
#include "cache/buffer.h"
#include "cache/cache_key.h"
#include "cache/cache_error.h"
#include "net/async_read_stream.h"
#include "net/proxy_conn.h"
#include "plgns/plugins.h"

using x3me::print_utils::print_lim_text;

namespace http
{
namespace detail
{
enum client_rbuf_size_idx : uint8_t
{
    client_rbuf_size_def = 0,
    client_rbuf_size_mid = 1,
    client_rbuf_size_max = 2,
};
enum origin_rbuf_size_idx : uint8_t
{
    origin_rbuf_size_def = 0,
    origin_rbuf_size_max = 1,
};
static constexpr bytes32_t min_csum_data_len = 1;
// The origin buffer is at least one block or more.
static_assert(
    min_csum_data_len < origin_rbuf_block_size,
    "We need to be able to keep all data for the comparison in the buffer");
static_assert(client_rbuf_block_size == 4_KB,
              "Must correspond to the below array constants");
static_assert(origin_rbuf_block_size == 8_KB,
              "Must correspond to the below array constants");
static constexpr bytes32_t client_rbuf_size[] = {4_KB, 8_KB, 16_KB};
static constexpr bytes32_t origin_rbuf_size[] = {8_KB, 16_KB};

static auto curr_block(const xutils::io_buff_reader& rdr) noexcept
{
    auto it = rdr.begin();
    return (it != rdr.end()) ? *it : xutils::io_buff_reader::block_t{};
}

static void inc_trans_id(id_tag::trans_id_t& id) noexcept
{
    static_assert(std::is_unsigned<id_tag::trans_id_t>::value,
                  "Overflow must not be undefined behavior");
    ++id;
    if (id == 0)
        id = 1;
}

static bytes32_t fill_vec_buffers(const xutils::io_buff_reader& ior,
                                  cache::const_buffers& vbuf) noexcept
{
    bytes32_t ret = 0;
    for (auto it = ior.begin(); it != ior.end(); ++it)
    {
        const auto block = *it;
        vbuf.emplace_back(block.data(), block.size());
        ret += block.size();
    }
    return ret;
}

static bool
compare_buffers(const xutils::io_buff_reader& ior,
                const x3me::mem_utils::array_view<uint8_t>& buff) noexcept
{
    X3ME_ENFORCE(ior.bytes_avail() == buff.size(),
                 "The provided buffers must have equal size");
    const uint8_t* pb = buff.data();
    for (const auto& block : ior)
    {
        if (::memcmp(pb, block.data(), block.size()) != 0)
            return false;
        pb += block.size();
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
namespace hhsm // HTTP Handler State Machine
{

// clang-format off
// Events
struct ev_cache_open_rd { net::proxy_conn* conn_; };
struct ev_compare_ok { net::proxy_conn* conn_; };
struct ev_compare_fail { net::proxy_conn* conn_; };
struct ev_cache_open_wr { net::proxy_conn* conn_; };
struct ev_cache_op_done { net::proxy_conn* conn_; };
struct ev_cache_op_next { net::proxy_conn* conn_; };
struct ev_cache_op_err { net::proxy_conn* conn_; };
struct ev_org_data { net::proxy_conn* conn_; };
struct ev_trans_completed { net::proxy_conn* conn_; };
struct ev_try_blind_tunnel { net::proxy_conn* conn_; };
struct ev_skip_trans {};
// clang-format on

struct sm_impl
{
    struct on_unexpected_event
    {
        template <typename Ev>
        void operator()(http_handler* h, const Ev& ev) noexcept;
    };

    auto operator()() noexcept
    {
        // clang-format off
        // Guards 
        auto has_cache_wr_data = [](http_handler* h)
        { 
            return h->has_cache_wr_data();
        };
        auto pend_blind_tunnel = [](http_handler* h)
        { 
            return h->pend_blind_tunnel(); 
        };
        // Actions
        auto skip_act = []{};
        auto pause_org_recv = [](http_handler* h, auto ev)
        {
            h->pause_org_recv(*ev.conn_);
        };
        auto resume_org_recv = [](http_handler* h, auto ev)
        {
            h->resume_org_recv(*ev.conn_);
        };
        auto consume_cache_hdrs_data = [](http_handler* h)
        {
            h->consume_cache_hdrs_data();
        };
        auto consume_cache_data = [](http_handler* h)
        {
            h->consume_cache_data();
        };
        auto cache_open_rd = [](http_handler* h, auto ev)
        { 
            h->cache_open_rd(*ev.conn_);
        };
        auto cache_read_compare = [](http_handler* h, auto ev)
        { 
            h->cache_read_compare(*ev.conn_);
        };
        auto cache_open_wr = [](http_handler* h, auto ev)
        { 
            h->cache_open_wr(*ev.conn_, false/*truncate*/);
        };
        auto cache_reopen_wr_truncate = [](http_handler* h, auto ev)
        { 
            h->cache_reopen_wr_truncate(*ev.conn_);
        };
        auto cache_write = [](http_handler* h, auto ev)
        { 
            h->cache_write(*ev.conn_); 
        };
        auto set_trans_completed = [](http_handler* h)
        {
            h->set_trans_completed();
        };
        auto fin_trans_send_next = [](http_handler* h, auto ev) 
        { 
            h->fin_trans_send_next(*ev.conn_); 
        };
        auto set_pend_blind_tunnel = [](http_handler* h)
        {
            h->set_pend_blind_tunnel();
        };
        auto start_blind_tunnel = [](http_handler* h, auto ev)
        {
            h->start_blind_tunnel(*ev.conn_);
        };
        auto rem_bpctrl_entry = [](http_handler* h)
        {
            h->rem_bpctrl_entry();
        };
        auto switch_org_stream = [](http_handler* h, auto ev)
        {
            h->switch_org_stream(*ev.conn_);
        };
        auto cache_close = [](http_handler* h) { h->cache_close(); };
        // States
        using namespace boost::sml;
        const auto wait_body_data_s = "wait_body_data"_s;
        const auto cache_open_rd_s  = "cache_open_rd"_s;
        const auto cache_compare_s  = "cache_compare"_s;
        const auto cache_open_wr_s  = "cache_open_wr"_s;
        const auto cache_read_s     = "cache_read"_s;
        const auto cache_write_s    = "cache_write"_s;
        const auto cache_idle_wr_s  = "cache_idle_wr"_s;
        const auto cache_closed_s   = "cache_closed"_s;
        // We don't stop/abort currently active cache write, if we want
        // to start blind tunnel. We first write all data available for
        // writing and then start the blind tunnel.
        // Note that the blind tunnel start needs to wait for a cache operation
        // to finish because the last, if read/write, uses the proxy_connection
        // buffers.
        return make_transition_table(
            *wait_body_data_s + event<ev_org_data> / consume_cache_hdrs_data,
            wait_body_data_s + event<ev_skip_trans> = cache_closed_s,
            // Consume too short transactions, received all at once.
            wait_body_data_s + event<ev_trans_completed> / 
                                    (consume_cache_data, fin_trans_send_next),
            // Open read phase
            wait_body_data_s + event<ev_cache_open_rd> / 
                            (pause_org_recv, cache_open_rd) = cache_open_rd_s,
            cache_open_rd_s + event<ev_cache_op_err> [!pend_blind_tunnel] /
                                (cache_close, resume_org_recv) = cache_closed_s,
            cache_open_rd_s + event<ev_cache_op_err> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            cache_open_rd_s + event<ev_cache_op_done> [!pend_blind_tunnel] /
                                        cache_read_compare = cache_compare_s,
            cache_open_rd_s + event<ev_cache_op_done> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            cache_open_rd_s + event<ev_cache_op_next> [!pend_blind_tunnel] /
                                (cache_close, cache_open_wr) = cache_open_wr_s,
            cache_open_rd_s + event<ev_cache_op_next> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            // Checksum phase
            cache_compare_s + event<ev_compare_ok> [!pend_blind_tunnel] /
                        (consume_cache_data, switch_org_stream) = cache_read_s,
            cache_compare_s + event<ev_compare_fail> [!pend_blind_tunnel] /
                                    cache_reopen_wr_truncate = cache_open_wr_s,
            cache_compare_s + event<ev_compare_ok> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            cache_compare_s + event<ev_compare_fail> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            cache_compare_s + event<ev_cache_op_err> [!pend_blind_tunnel] /
                                (cache_close, resume_org_recv) = cache_closed_s,
            cache_compare_s + event<ev_cache_op_err> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            // Open write phase
            cache_open_wr_s + event<ev_cache_op_err> [!pend_blind_tunnel] /
                                (cache_close, resume_org_recv) = cache_closed_s,
            cache_open_wr_s + event<ev_cache_op_err> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            cache_open_wr_s + event<ev_cache_op_done> 
                                [!pend_blind_tunnel && has_cache_wr_data] / 
                                (resume_org_recv, cache_write) = cache_write_s,
            cache_open_wr_s + event<ev_cache_op_done> 
                                [!pend_blind_tunnel && !has_cache_wr_data] / 
                                            resume_org_recv = cache_idle_wr_s,
            cache_open_wr_s + event<ev_cache_op_done> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            // Cache read phase
            cache_read_s + event<ev_org_data> / consume_cache_data,
            cache_read_s + event<ev_trans_completed> / set_trans_completed,
            cache_read_s + event<ev_cache_op_done> = cache_closed_s,
            // Write data phase
            cache_idle_wr_s + event<ev_org_data> / cache_write = cache_write_s,
            cache_write_s + event<ev_org_data> / skip_act,
            cache_write_s + event<ev_trans_completed> / set_trans_completed,
            cache_write_s + event<ev_cache_op_err> [!pend_blind_tunnel] /
                                                cache_close = cache_closed_s,
            cache_write_s + event<ev_cache_op_err> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            cache_write_s + event<ev_cache_op_next>
                            [!pend_blind_tunnel && has_cache_wr_data] /
                                                    cache_write = cache_write_s,
            cache_write_s + event<ev_cache_op_next>
                [!pend_blind_tunnel && !has_cache_wr_data] = cache_idle_wr_s,
            cache_write_s + event<ev_cache_op_next> [pend_blind_tunnel] /
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            // This line is a bit of a hack. See ev_trans_completed usage.
            cache_idle_wr_s + event<ev_trans_completed> / 
                        (cache_close, fin_trans_send_next) = wait_body_data_s,
            // Handle blind tunnel in the different states 
            cache_open_rd_s + event<ev_try_blind_tunnel> / 
                                    (rem_bpctrl_entry, set_pend_blind_tunnel),
            cache_compare_s + event<ev_try_blind_tunnel> / 
                                    (rem_bpctrl_entry, set_pend_blind_tunnel),
            cache_open_wr_s + event<ev_try_blind_tunnel> /
                                    (rem_bpctrl_entry, set_pend_blind_tunnel),
            cache_write_s + event<ev_try_blind_tunnel> /
                                    (rem_bpctrl_entry, set_pend_blind_tunnel),
            cache_idle_wr_s + event<ev_try_blind_tunnel> / 
                            (cache_close, start_blind_tunnel) = cache_closed_s,
            // We allow starting blind tunnel immediately if we read from the
            // cache, because we have provided the async_read_stream to the
            // lower level.
            cache_read_s + event<ev_try_blind_tunnel> / 
                                            start_blind_tunnel = cache_closed_s,
            wait_body_data_s + event<ev_try_blind_tunnel> / start_blind_tunnel =
                                                                cache_closed_s,
            cache_closed_s + event<ev_try_blind_tunnel> / start_blind_tunnel,
            // Interactions after the cache handle has been closed
            cache_closed_s + event<ev_skip_trans> / skip_act,
            cache_closed_s + event<ev_org_data> / consume_cache_data,
            cache_closed_s + event<ev_trans_completed> / fin_trans_send_next = 
                                                            wait_body_data_s
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
            std::cerr << "BUG in Http_handler state machine\n";
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

    bool is_reading_from_cache() const noexcept
    {
        using namespace boost::sml;
        return base_t::is("cache_read"_s);
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

} // namespace hhsm
////////////////////////////////////////////////////////////////////////////////

http_handler::http_handler(const id_tag& tag,
                           cache::object_distributor& cod,
                           io_service_t& ios,
                           net_thread_id_t net_tid,
                           all_stats& sts,
                           http_bp_ctl& bp_ctl) noexcept
    : cache_handle_(cod, ios),
      csm_(this),
      all_stats_(sts),
      bp_ctrl_(bp_ctl),
      tag_(tag),
      client_rbuf_size_idx_(detail::client_rbuf_size_def),
      origin_rbuf_size_idx_(detail::origin_rbuf_size_def),
      net_thread_id_(net_tid)
{
    tag_.set_module_id(id_tag::module::http);
    XLOG_DEBUG(no_trans_tag(), "Http_handler construct");
}

http_handler::~http_handler() noexcept
{
    auto* pm = plgns::plugins::instance;
    for (auto& t : transactions_)
    {
        all_stats_.var_stats_.cnt_all_trans_hit_ += t.is_cache_hit();
        pm->on_transaction_end(net_thread_id_, t);
        t.log_before_destroy();
    }
    XLOG_INFO(no_trans_tag(), "Http_handler destruct");
    rem_bpctrl_entry();
}

void http_handler::init(net::proxy_conn& conn) noexcept
{
    using namespace detail;
    const auto crbuf_size = client_rbuf_size[client_rbuf_size_idx_];
    const auto orbuf_size = origin_rbuf_size[origin_rbuf_size_idx_];

    XLOG_INFO(no_trans_tag(), "Http_handler::init. Client_recv_buff {} bytes. "
                              "Origin_recv_buff {} bytes",
              crbuf_size, orbuf_size);

    conn.register_client_reader(client_rdr_);
    conn.register_origin_reader(origin_rdr_);
    conn.register_origin_reader(org_cache_rdr_);

    conn.expand_client_recv_buff(crbuf_size);
    conn.expand_origin_recv_buff(orbuf_size);
}

void http_handler::on_origin_pre_connect(net::proxy_conn& conn) noexcept
{
    XLOG_INFO(org_trans_tag(), "Http_handler. BPCTRL add entry");
    X3ME_ASSERT((flags_ & flags::bpctrl_entry_added) == 0);
    if (auto err = bp_ctrl_.add_del_entry(tag_.user_endpoint(),
                                          tag_.server_endpoint()))
    {
        XLOG_ERROR(org_trans_tag(), "BPCTRL error adding entry. {}",
                   err.message());
        csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
    }
    else
    {
        flags_ |= flags::bpctrl_entry_added;
        all_stats_.var_stats_.cnt_bpctrl_entries_ += 1;
    }
}

void http_handler::on_switched_stream_eof(net::proxy_conn& conn) noexcept
{
    if (!transactions_.empty())
    {
        auto& tr = transactions_[0];
        if (!tr.resp_completed() && (tr.resp_bytes() > 0))
        {
            XLOG_ERROR(org_trans_tag(), "Wrong content. Read less cache "
                                        "content than expected. Hdr bytes {}. "
                                        "Msg bytes {}",
                       tr.resp_hdrs_bytes(), tr.resp_bytes());
        }
    }
    csm_->process_event(hhsm::ev_cache_op_done{&conn});
    conn.enqueue_close_client();
}

void http_handler::on_client_data(net::proxy_conn& conn) noexcept
{
    auto enqueue_trans = [this]
    {
        transactions_.emplace_back(cln_trans_tag());
        ++all_stats_.var_stats_.cnt_all_trans_;
    };
    if (transactions_.empty() || transactions_.back().req_completed())
        enqueue_trans();
    const auto prev_bytes   = transactions_[0].req_bytes();
    const auto avail        = client_rdr_.bytes_avail();
    bytes32_t consumed      = 0;
    bytes32_t curr_consumed = 0; // Consumed from the current block
    XLOG_DEBUG(cln_trans_tag(), "Http_handler::on_client_data. Avail bytes {}",
               avail);
    for (auto it = client_rdr_.begin(); it != client_rdr_.end();)
    {
        const auto blk = *it;
        auto& trans    = transactions_.back();

        auto data       = reinterpret_cast<const bytes8_t*>(blk.data());
        const auto size = blk.size() - curr_consumed;
        const auto res = trans.on_req_data(data + curr_consumed, size);
        switch (res.res_)
        {
        case http_trans::res::proceed:
            consumed += res.consumed_;
            assert(size == res.consumed_);
            curr_consumed = 0;
            ++it;
            break;
        case http_trans::res::complete:
            consumed += res.consumed_;
            trans.update_req_stats(all_stats_);
            detail::inc_trans_id(cln_trans_id_);
            // If there is more data, enqueue new request to consume them
            if (avail > consumed)
                enqueue_trans();
            if (res.consumed_ < size)
            {
                curr_consumed += res.consumed_;
                assert(curr_consumed < blk.size());
                // We stay to the current block in order to consume it.
            }
            else
            {
                assert(size == res.consumed_);
                curr_consumed = 0;
                ++it;
            }
            break;
        case http_trans::res::unsupported:
            XLOG_INFO(cln_trans_tag(),
                      "Http_handler::on_client_data. Trying blind tunnel "
                      "on unsupported request");
            ++all_stats_.var_stats_.cnt_all_unsupported_req_;
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return;
        case http_trans::res::error:
        {
            XLOG_INFO(cln_trans_tag(),
                      "Http_handler::on_client_data. Trying blind tunnel "
                      "on invalid request data");
            ++all_stats_.var_stats_.cnt_all_error_req_;
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return;
        }
        }
    }
    X3ME_ASSERT(consumed == avail, "The above logic is wrong");
    client_rdr_.consume(consumed);

    expand_client_recv_buff_if_needed(transactions_.back(), conn);

    const auto curr_bytes = transactions_[0].req_bytes();
    if (curr_bytes > prev_bytes)
    {
        // If we hasn't started sending the request we need to coordinated the
        // action with the caching state machine. We don't want to start sending
        // new request and more importantly receiving origin data for the new
        // response before we finish writing the data of the previous response.
        // However, if we have started sending the request i.e. it's been
        // allowed from the caching state machine we want to finish it.
        XLOG_DEBUG(org_trans_tag(), "Http_handler::on_client_data. Continue "
                                    "sending request to origin. {} bytes",
                   curr_bytes - prev_bytes);
        conn.send_to_origin(curr_bytes - prev_bytes);
    }
}

void http_handler::on_client_recv_eof(net::proxy_conn& conn) noexcept
{
    XLOG_INFO(cln_trans_tag(),
              "Http_handler. EOF from client. Pending transactions {}. "
              "Requesting shutdown origin send",
              transactions_.size());
    // There could be no current transaction if it's been just completed
    // with a response from the origin side.
    if (!transactions_.empty())
        transactions_.back().on_req_end_of_stream();
    conn.enqueue_shutdown_origin_send();
    // If we don't have pending transactions, we still can not enqueue close
    // of the origin connection because the transaction could have been
    // finished but the server may send data to the client after the response.
    // We can't close the origin connection even if we haven't received any
    // data from the client, because this could be non HTTP protocol which
    // expects the client to shutdown the send part of the connection and
    // then the server to start sending data to the client ...
}

void http_handler::on_client_recv_err(net::proxy_conn& conn) noexcept
{
    // Receive or send error means that the client has broken the
    // connection. Let's not try to be too smart, we can't gain anything
    // from this connection anymore, so go to blind tunnel and let the
    // proxy connection send all pending data where it can.
    if (!transactions_.empty())
    {
        XLOG_INFO(cln_trans_tag(),
                  "Http_handler. Trying blind tunnel on client recv "
                  "error. Pending transactions {}. Curr_trans {}",
                  transactions_.size(), transactions_.back());
    }
    else
    {
        XLOG_INFO(cln_trans_tag(),
                  "Http_handler. Trying blind tunnel on client recv "
                  "error. No pending transactions");
    }
    // The blind tunnel will close the origin connection after it flushes
    // all pending data on it.
    csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
}

void http_handler::on_client_send_err(net::proxy_conn& conn) noexcept
{
    // Receive or send error means that the client has broken the
    // connection. Let's not try to be too smart, we can't gain anything
    // from this connection anymore, so go to blind tunnel and let the
    // proxy connection send all pending data where it can.
    // We send to the client the data from the origin, thus we want to
    // log this with the origin transaction tag
    if (!transactions_.empty())
    {
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. Trying blind tunnel on client send "
                  "error. Pending transactions {}. Curr_trans {}",
                  transactions_.size(), transactions_.back());
    }
    else
    {
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. Trying blind tunnel on client send "
                  "error. No pending transactions");
    }
    // The blind tunnel will close the origin connection after it flushes
    // all pending data on it.
    csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
}

void http_handler::on_origin_data(net::proxy_conn& conn) noexcept
{
    X3ME_ASSERT(
        !(flags_ & flags::origin_recv_paused),
        "We must not receive data while the origin connection is paused");

    const auto avail_bytes = origin_rdr_.bytes_avail();

    if (!is_resp_expected(conn))
        return;

    if (!consume_curr_resp_data(conn))
        return;

    if (!set_curr_trans_backpressure_params(conn))
        return;

    if (!process_curr_trans_resp(conn))
        return;

    // Current transaction could have been completed and erased.
    // Thus we need the check before accessing transactions_.
    if (!transactions_.empty())
        expand_origin_recv_buff_if_needed(transactions_.front(), conn);

    conn.send_to_client(avail_bytes);
}

void http_handler::on_origin_recv_eof(net::proxy_conn& conn) noexcept
{
    // There could be no current transaction if it's been just completed
    // with a response from the origin side.
    if (!transactions_.empty())
    {
        auto& trans = transactions_.back();
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. EOF from origin. Requesting "
                  "shutdown client send. Starting blind "
                  "tunnel. Pending transactions {}. Curr_trans {}",
                  transactions_.size(), trans);
        trans.on_resp_end_of_stream();
    }
    else
    {
        // It's normal the server to close the connection after it sends all
        // the data. Thus we log as debug.
        XLOG_DEBUG(org_trans_tag(),
                   "Http_handler. EOF from origin. No pending transactions. "
                   "Requesting shutdown client send. Starting blind tunnel");
    }
    conn.enqueue_shutdown_client_send();
    // We won't receive anything more from the origin i.e. we can't do
    // any response based caching. Thus we go to blind tunnel mode.
    csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
}

void http_handler::on_origin_recv_err(net::proxy_conn& conn) noexcept
{
    // Receive or send error means that the origin has broken the
    // connection.
    if (!transactions_.empty())
    {
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. Trying blind tunnel on origin recv "
                  "error. Pending transactions {}. Curr_trans {}",
                  transactions_.size(), transactions_.back());
    }
    else
    {
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. Trying blind tunnel on origin recv "
                  "error. No pending transactions");
    }
    // The blind tunnel will close the client connection after it flushes
    // all pending data on it.
    csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
}

void http_handler::on_origin_send_err(net::proxy_conn& conn) noexcept
{
    // Receive or send error means that the origin has broken the
    // connection. Let's not try to be too smart, we can't gain anything
    // from this connection anymore, so go to blind tunnel and let the
    // proxy connection send all pending data where it can.
    if (!transactions_.empty())
    {
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. Trying blind tunnel on origin send "
                  "error. Pending transactions {}. Curr_trans {}",
                  transactions_.size(), transactions_.back());
    }
    else
    {
        XLOG_INFO(org_trans_tag(),
                  "Http_handler. Trying blind tunnel on origin send "
                  "error. No pending transactions");
    }
    // The blind tunnel will close the client connection after it flushes
    // all pending data on it.
    csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
}

////////////////////////////////////////////////////////////////////////////////

bool http_handler::is_resp_expected(net::proxy_conn& conn) noexcept
{
    bool ret = true;
    if (transactions_.empty())
    {
        const auto blk = detail::curr_block(origin_rdr_);
        XLOG_INFO(org_trans_tag(),
                  "Http_handler::is_resp_expected. Trying blind tunnel "
                  "on response first. No pending_trans. Resp_data:\n{}",
                  print_lim_text(blk, 50U));
        ++all_stats_.var_stats_.cnt_server_talks_first_;
        csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
        ret = false;
    }
    else if (!transactions_[0].req_completed())
    {
        // We receive response data before we have received complete
        // request.
        // Note that we send the request parts without holding them i.e.
        // if we have received a complete request we have sent it.
        // We assume that this is a case when the server talks first.
        const auto blk = detail::curr_block(origin_rdr_);
        XLOG_INFO(org_trans_tag(),
                  "Http_handler::is_resp_expected. Trying blind tunnel "
                  "on response too early. Pending_trans {}. Resp_data:\n{}",
                  transactions_[0], print_lim_text(blk, 50U));
        ++all_stats_.var_stats_.cnt_server_talks_early_;
        csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
        ret = false;
    }
    return ret;
}

bool http_handler::consume_curr_resp_data(net::proxy_conn& conn) noexcept
{
    auto& trans = transactions_[0];
    auto& rdr = origin_rdr_;
    XLOG_DEBUG(org_trans_tag(),
               "Http_handler::consume_curr_resp_data. Avail bytes {}",
               rdr.bytes_avail());
    const bool had_hdrs = trans.resp_hdrs_completed();
    bool do_break       = false;
    bytes32_t consumed = 0;
    for (auto it = rdr.begin(); ((it != rdr.end()) && !do_break); ++it)
    {
        const auto blk = *it;
        auto data      = reinterpret_cast<const bytes8_t*>(blk.data());
        const auto res = trans.on_resp_data(data, blk.size());
        switch (res.res_)
        {
        case http_trans::res::proceed:
            assert(res.consumed_ == blk.size());
            consumed += res.consumed_;
            break;
        case http_trans::res::complete:
            assert(res.consumed_ <= blk.size());
            consumed += res.consumed_;
            do_break = true;
            break;
        case http_trans::res::unsupported:
            XLOG_INFO(
                org_trans_tag(),
                "Http_handler::consume_curr_resp_data. Trying blind tunnel "
                "on unsupported response");
            ++all_stats_.var_stats_.cnt_all_unsupported_resp_;
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return false;
        case http_trans::res::error:
        {
            XLOG_INFO(
                org_trans_tag(),
                "Http_handler::consume_curr_resp_data. Trying blind tunnel "
                "on invalid response data");
            ++all_stats_.var_stats_.cnt_all_error_resp_;
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return false;
        }
        }
    }
    rdr.consume(consumed);

    adjust_trans_state(conn, trans, (!had_hdrs && trans.resp_completed()));

    return true;
}

void http_handler::adjust_trans_state(net::proxy_conn& conn,
                                      http_trans& trans,
                                      bool completed_at_once) noexcept
{
    if (!trans.in_http_tunnel() &&
        (trans.resp_bytes() > constants::bpctrl_window_size) &&
        (trans.resp_body_bytes() < detail::min_csum_data_len))
    {
        all_stats_.var_stats_.cnt_ccompare_skip_ += 1;
        trans.force_http_tunnel();
        XLOG_WARN(org_trans_tag(), "Can't collect enough content to compare. "
                                   "Entered http_tunnel. Trans {}",
                  trans);
    }
    // Saying to the state machine to skip the transaction before the
    // event for more data from the origin is important.
    // It ensures that the state machine knows if to consume the data only
    // from the headers or all of them (in case of HTTP tunnel).
    // If all transaction data is received at once it doesn't make much sense
    // to try store it in the cache. It's too small, so skip it.
    if (trans.in_http_tunnel())
        csm_->process_event(hhsm::ev_skip_trans{});
    else if (completed_at_once)
        csm_->process_event(hhsm::ev_skip_trans{});
    // Here we inform the state machine for the new received data. It may
    // consume the headers data, the whole data, if the transaction should
    // be skipped, or it may wait for/issue cache write.
    csm_->process_event(hhsm::ev_org_data{&conn});
}

bool http_handler::set_curr_trans_backpressure_params(
    net::proxy_conn& conn) noexcept
{
    auto& trans = transactions_[0];
    if (flags_ & flags::tr_bpctrl_params_set) // Set them only once per trans
        return true;

    if (trans.is_chunked())
    {
        X3ME_ASSERT(trans.in_http_tunnel(),
                    "Must be in http tunnel if it's chunked");
        X3ME_ASSERT((flags_ & flags::bpctrl_entry_added),
                    "The entry must have been added");
        XLOG_INFO(org_trans_tag(), "BPCTRL control set chunked");
        if (auto err = bp_ctrl_.chunked_end(tag_.user_endpoint(),
                                            tag_.server_endpoint()))
        {
            XLOG_ERROR(org_trans_tag(), "BPCTRL error set chunked. {}",
                       err.message());
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return false;
        }
        flags_ |= flags::tr_bpctrl_params_set;
        return true;
    }

    if (trans.in_http_tunnel())
    {
        if (const auto clen = trans.resp_content_len())
        {
            if (set_bpctrl_content_len(*clen))
            {
                flags_ |= flags::tr_bpctrl_params_set;
                return true;
            }
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return false;
        }
    }

    // The response headers may still not be completed, or
    // they are completed but without information about 'Content-Length'
    // or 'Transfer-Encoding'. In either case we don't need to do
    // anything else here (IMO). Either the transaction is done and we
    // won't receive any more data for it, or we'll receive unexpected
    // data for it and we'll go to blind tunnel mode.
    return true;
}

bool http_handler::set_bpctrl_content_len(bytes64_t clen) noexcept
{
    X3ME_ASSERT((flags_ & flags::bpctrl_entry_added),
                "The entry must have been added");
    XLOG_INFO(org_trans_tag(), "BPCTRL set content length to {}", clen);
    if (auto err = bp_ctrl_.content_len(clen, tag_.user_endpoint(),
                                        tag_.server_endpoint()))
    {
        XLOG_ERROR(org_trans_tag(), "BPCTRL error set content length. {}",
                   err.message());
        return false;
    }
    return true;
}

void http_handler::set_bpctrl_curr_trans_clen() noexcept
{
    X3ME_ASSERT(!transactions_.empty(), "There must be at least one "
                                        "transaction with unfinished "
                                        "response data");
    auto& tr        = transactions_[0];
    const auto clen = tr.resp_content_len();
    X3ME_ENFORCE(!tr.in_http_tunnel() &&
                     !(flags_ & flags::tr_bpctrl_params_set) && clen,
                 "The content-length be present and must have not been set to "
                 "the BPCTRL module prior this function is called");
    if (set_bpctrl_content_len(*clen))
    {
        flags_ |= flags::tr_bpctrl_params_set;
    }
}

bool http_handler::process_curr_trans_resp(net::proxy_conn& conn) noexcept
{
    auto& trans = transactions_[0];
    if (trans.resp_completed())
    {
        // Note that we can end up here without first setting the content
        // length to the BPCTRL module. This may happen if the transaction
        // is too short. In this case it's not needed to set the content
        // length to the module, it's already too late.
        X3ME_ASSERT(trans.req_completed(), "The request must have been "
                                           "completed if the response is "
                                           "completed");
        XLOG_DEBUG(org_trans_tag(),
                   "Http_handler::process_curr_trans_resp. Transaction "
                   "completed. Cnt_transactions {}. Avail bytes {}",
                   transactions_.size(), origin_rdr_.bytes_avail());
        // Note that 'trans' is invalid after this point
        if (origin_rdr_.bytes_avail() == 0)
            csm_->process_event(hhsm::ev_trans_completed{&conn});
        else
        {
            if (!csm_->is_reading_from_cache())
            {
                const auto blk = detail::curr_block(origin_rdr_);
                XLOG_INFO(
                    org_trans_tag(),
                    "Http_handler::process_curr_trans_resp. Start "
                    "blind tunnel on after-end response data. Resp_data:\n{}",
                    print_lim_text(blk, 50U));
                flags_ |= flags::tr_after_end_data;
            }
            else
            {
                XLOG_ERROR(
                    org_trans_tag(),
                    "Wrong content. Reading more cache content than expected");
            }
            csm_->process_event(hhsm::ev_try_blind_tunnel{&conn});
            return false;
        }
    }
    else if (!trans.in_http_tunnel() && !(flags_ & flags::tr_caching_started))
    {
        // A valid cache key is returned only if the response headers are
        // completed and the transaction is not in http_tunnel.
        if (const auto ckey = trans.get_cache_key())
        {
            const bytes32_t skip_len = 0;
            if (!cache::rw_op_allowed(*ckey, skip_len))
            {
                flags_ |= flags::tr_caching_started;
                XLOG_DEBUG(org_trans_tag(), "Won't start cache read operation "
                                            "for CKey {}. Skip_bytes {}",
                           *ckey, skip_len);
                // We are going to skip this transaction.
                // Start silently consuming the data.
                set_bpctrl_curr_trans_clen();
                csm_->process_event(hhsm::ev_skip_trans{});
                csm_->process_event(hhsm::ev_org_data{&conn});
            }
            else
            {
                const auto csum_bytes = trans.resp_body_bytes();
                if (csum_bytes >= detail::min_csum_data_len)
                {
                    flags_ |= flags::tr_caching_started;
                    plgns::plugins::instance->on_before_cache_open_read(
                        net_thread_id_, trans);
                    csm_->process_event(hhsm::ev_cache_open_rd{&conn});
                }
                else
                {
                    XLOG_DEBUG(org_trans_tag(), "Not enough data ({} bytes) to "
                                                "check against cached content. "
                                                "Continue org_receive",
                               csum_bytes);
                }
            }
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool http_handler::has_cache_wr_data() const noexcept
{
    return org_cache_rdr_.bytes_avail() > 0;
}

bool http_handler::trans_completed() const noexcept
{
    return flags_ & flags::transaction_completed;
}

void http_handler::set_trans_completed() noexcept
{
    flags_ |= flags::transaction_completed;
}

bool http_handler::pend_blind_tunnel() const noexcept
{
    return flags_ & flags::pending_blind_tunnel;
}

void http_handler::set_pend_blind_tunnel() noexcept
{
    flags_ |= flags::pending_blind_tunnel;
}

void http_handler::start_blind_tunnel(net::proxy_conn& conn) noexcept
{
    rem_bpctrl_entry();
    // We unregister readers explicitly so that the proxy connection
    // can start using the io_buffers as soon as it decides.
    // If we don't unregister them here it may need to wait until the
    // handler is destroyed. The readers are automatically unregistered on
    // destruction.
    conn.unregister_client_reader(client_rdr_);
    conn.unregister_origin_reader(origin_rdr_);
    conn.unregister_origin_reader(org_cache_rdr_);
    conn.start_blind_tunnel();
}

void http_handler::switch_org_stream(net::proxy_conn& conn) noexcept
{
    X3ME_ASSERT(cache_handle_.is_open(), "The cache handle must have been "
                                         "successfully opened for reading "
                                         "before calling this function");
    rem_bpctrl_entry();
    net::async_read_stream::impl_type<async_cache_reader> impl;
    conn.switch_org_stream(
        net::async_read_stream{impl, std::move(cache_handle_)});
    conn.resume_origin_recv();
    flags_ &= ~flags::origin_recv_paused;
}

////////////////////////////////////////////////////////////////////////////////

void http_handler::pause_org_recv(net::proxy_conn& conn) noexcept
{
    conn.pause_origin_recv();
    flags_ |= flags::origin_recv_paused;
}

void http_handler::resume_org_recv(net::proxy_conn& conn) noexcept
{
    set_bpctrl_curr_trans_clen();
    conn.resume_origin_recv();
    flags_ &= ~flags::origin_recv_paused;
}

void http_handler::fin_trans_send_next(net::proxy_conn& conn) noexcept
{
    X3ME_ASSERT(!transactions_.empty(), "There must be at least one "
                                        "transaction when in this state");
    auto& trans = transactions_[0];
    X3ME_ASSERT(trans.resp_completed(),
                "The transaction must have been completed when in this state");
    const bool in_http_tunnel = trans.in_http_tunnel();
    all_stats_.var_stats_.cnt_all_cacheable_trans_ += !in_http_tunnel;
    all_stats_.var_stats_.cnt_all_http_tunnel_trans_ += in_http_tunnel;
    all_stats_.var_stats_.cnt_all_trans_hit_ += trans.is_cache_hit();
    detail::inc_trans_id(org_trans_id_);
    trans.update_resp_stats(all_stats_);
    plgns::plugins::instance->on_transaction_end(net_thread_id_, trans);
    trans.log_before_destroy();
    transactions_.erase(transactions_.begin());
    // The 'trans' reference is no longer valid after this point
    reset_per_trans_flags();
    // This flag may or may not be set when we enter this function.
    // Sometimes we set the flag as pending, other times we don't need to
    // set the flag and call the function directly depending on the cache state.
    flags_ &= ~flags::transaction_completed;
    if (!transactions_.empty() && (transactions_[0].req_bytes() > 0))
    {
        const auto bytes = transactions_[0].req_bytes();
        XLOG_DEBUG(org_trans_tag(), "Send next request to origin. {} bytes",
                   bytes);
        conn.send_to_origin(bytes);
    }
}

////////////////////////////////////////////////////////////////////////////////

void http_handler::consume_cache_hdrs_data() noexcept
{
    X3ME_ASSERT(!transactions_.empty(), "There must be at least one "
                                        "transaction when in this state");
    const auto& trans = transactions_[0];
    auto avail = org_cache_rdr_.bytes_avail();
    if (trans.resp_hdrs_completed())
    { // Consume the remaining header bytes, if any
        const auto to_skip = trans.resp_body_bytes();
        X3ME_ASSERT(avail >= to_skip);
        avail -= to_skip;
    }
    else
    {
        X3ME_ASSERT(avail <= trans.resp_hdrs_bytes());
    }
    if (avail > 0)
    {
        XLOG_TRACE(org_trans_tag(),
                   "Consuming {} header bytes from origin cache reader", avail);
        org_cache_rdr_.consume(avail);
    }
}

void http_handler::consume_cache_data() noexcept
{
    const auto avail = org_cache_rdr_.bytes_avail();
    XLOG_TRACE(org_trans_tag(),
               "Consuming all available {} bytes from origin cache reader",
               avail);
    org_cache_rdr_.consume(avail);
}

////////////////////////////////////////////////////////////////////////////////

void http_handler::cache_open_rd(net::proxy_conn& conn) noexcept
{
    X3ME_ASSERT(!transactions_.empty(), "There must be at least one "
                                        "transaction with unfinished "
                                        "response data");
    const auto& trans = transactions_[0];
    const auto ckey = trans.get_cache_key();
    X3ME_ASSERT(ckey, "The transaction must have be valid, not in HTTP tunnel "
                      "and with received response headers");
    XLOG_DEBUG(org_trans_tag(), "Start cache read. CKey {}", *ckey);
    const bytes32_t skip_len = 0;
    cache_handle_.async_open_read(
        *ckey, skip_len,
        [ alive = conn.shared_from_this(), this ](const err_code_t& err)
        {
            if (!err)
            {
                csm_->process_event(hhsm::ev_cache_op_done{alive.get()});
            }
            else if (err == cache::object_not_present)
            {
                X3ME_ASSERT(!transactions_.empty(),
                            "There must be at least one "
                            "transaction with unfinished "
                            "response data");
                auto& trans = transactions_[0];
                trans.set_cache_miss();
                const auto ckey = trans.get_cache_key();
                // No skip bytes when writing. Thus we need to check again.
                if (cache::rw_op_allowed(*ckey, 0U))
                    csm_->process_event(hhsm::ev_cache_op_next{alive.get()});
                else
                {
                    XLOG_DEBUG(org_trans_tag(),
                               "Won't start cache write operation for CKey {}",
                               *ckey);
                    consume_cache_wr_data(*alive, org_cache_rdr_.bytes_avail());
                    // This is not exactly an error but doesn't want to
                    // extend the state machine.
                    csm_->process_event(hhsm::ev_cache_op_err{alive.get()});
                }
            }
            else
            {
                X3ME_ASSERT(err != cache::already_open,
                            "Wrong state machine. Must not try open when "
                            "already open");
                XLOG_ERROR(org_trans_tag(), "Cache open read error. {}",
                           err.message());
                consume_cache_wr_data(*alive, org_cache_rdr_.bytes_avail());
                csm_->process_event(hhsm::ev_cache_op_err{alive.get()});
            }
        });
}

void http_handler::cache_read_compare(net::proxy_conn& conn) noexcept
{
    // Don't read from the cache already received body data
    X3ME_ASSERT(!transactions_.empty(), "There must be at least one "
                                        "transaction with unfinished "
                                        "response data");
    const auto recv_body = transactions_[0].resp_body_bytes();
    const auto read_body = org_cache_rdr_.bytes_avail();
    X3ME_ENFORCE(recv_body == read_body,
                 "Different body bytes detected by the "
                 "http transaction logic and the io_buffer reader logic");
    XLOG_DEBUG(org_trans_tag(), "Cache_read_compare. Start reading {} bytes",
               read_body);
    auto buff = boost::make_shared<uint8_t[]>(read_body);
    cache_handle_.async_read(
        cache::buffer(buff.get(), read_body),
        [ alive = conn.shared_from_this(), this, buff ](const err_code_t& err,
                                                        bytes32_t read_len)
        {
            if (!err)
            {
                X3ME_ASSERT(!transactions_.empty(), "There must be at least "
                                                    "one transaction with "
                                                    "unfinished response data");
                auto& trans = transactions_[0];
                all_stats_.var_stats_.bytes_ccompare_ += read_len;
                using x3me::mem_utils::make_array_view;
                // We'll see from the stats for the average compared size
                // if this needs to be done asynchronously or not.
                if (compare_buffers(org_cache_rdr_,
                                    make_array_view(buff.get(), read_len)))
                {
                    all_stats_.var_stats_.cnt_ccompare_ok_ += 1;
                    trans.set_cache_hit();
                    trans.set_origin_resp_bytes(trans.resp_bytes());
                    XLOG_DEBUG(org_trans_tag(),
                               "Cache_read_compare OK. Compared {} bytes",
                               read_len);
                    csm_->process_event(hhsm::ev_compare_ok{alive.get()});
                }
                else
                {
                    all_stats_.var_stats_.cnt_ccompare_fail_ += 1;
                    trans.set_cache_csum_miss();
                    XLOG_WARN(
                        org_trans_tag(),
                        "Cache_read_compare failed. Compared {} bytes. URL: {}",
                        read_len, trans.req_url());
                    csm_->process_event(hhsm::ev_compare_fail{alive.get()});
                }
            }
            else if (err != cache::operation_aborted)
            {
                XLOG_ERROR(org_trans_tag(), "Cache_read_compare error. {}",
                           err.message());
                consume_cache_wr_data(*alive, org_cache_rdr_.bytes_avail());
                csm_->process_event(hhsm::ev_cache_op_err{alive.get()});
            }
        });
}

void http_handler::cache_reopen_wr_truncate(net::proxy_conn& conn) noexcept
{
    // We need to be sure that the cache_handle is actually closed, before
    // trying to open/truncate it for writing, because the operation fails
    // if there are active readers, and we are one of them until the actual
    // close operation.
    XLOG_TRACE(org_trans_tag(), "Start async_close cache_handle");
    cache_handle_.async_close(
        [ alive = conn.shared_from_this(), this ](const err_code_t& err)
        {
            if (!err)
            {
                XLOG_TRACE(org_trans_tag(), "Cache_handle closed");
                cache_open_wr(*alive, true /*truncate object*/);
            }
            else
            {
                XLOG_WARN(org_trans_tag(), "Error closing cache. {}",
                          err.message());
                consume_cache_wr_data(*alive, org_cache_rdr_.bytes_avail());
                csm_->process_event(hhsm::ev_cache_op_err{alive.get()});
            }
        });
}

void http_handler::cache_open_wr(net::proxy_conn& conn,
                                 bool truncate_obj) noexcept
{
    X3ME_ASSERT(!transactions_.empty(), "There must be at least one "
                                        "transaction with unfinished "
                                        "response data");
    const auto ckey = transactions_[0].get_cache_key();
    X3ME_ASSERT(ckey, "The transaction must have be valid, not in HTTP tunnel "
                      "and with received response headers");
    XLOG_DEBUG(org_trans_tag(), "Start cache write. Entry {}. Truncate_obj {}",
               *ckey, truncate_obj);
    cache_handle_.async_open_write(
        *ckey, truncate_obj,
        [ alive = conn.shared_from_this(), this ](const err_code_t& err)
        {
            if (!err)
            {
                XLOG_DEBUG(org_trans_tag(), "Cache opened for write");
                csm_->process_event(hhsm::ev_cache_op_done{alive.get()});
            }
            else if (err != cache::operation_aborted)
            {
                X3ME_ASSERT(err != cache::already_open,
                            "Wrong state machine. Must not try open when "
                            "already open");
                XLOG_WARN(org_trans_tag(), "Error opening cache for write. {}",
                          err.message());
                consume_cache_wr_data(*alive, org_cache_rdr_.bytes_avail());
                csm_->process_event(hhsm::ev_cache_op_err{alive.get()});
            }
        });
}

void http_handler::cache_write(net::proxy_conn& conn) noexcept
{
    cache::const_buffers bufs;
    const auto avail = fill_vec_buffers(org_cache_rdr_, bufs);
    X3ME_ASSERT(avail > 0, "There must be some bytes available in the reader "
                           "if this function is called");
    XLOG_DEBUG(org_trans_tag(), "Async cache write. Bytes {}", avail);
    cache_handle_.async_write(
        std::move(bufs), [ alive = conn.shared_from_this(),
                           this ](const err_code_t& err, bytes32_t written)
        {
            const bool tr_completed    = trans_completed();
            const bool in_blind_tunnel = pend_blind_tunnel();
            if (!err)
            {
                XLOG_DEBUG(org_trans_tag(), "Async cache write done. "
                                            "Written {}. Trans_completed "
                                            "{}",
                           written, tr_completed);
                consume_cache_wr_data(*alive, written);
                csm_->process_event(hhsm::ev_cache_op_next{alive.get()});
            }
            else if (err != cache::operation_aborted)
            {
                X3ME_ASSERT(err != cache::already_open,
                            "Wrong state machine. Must not try open when "
                            "already open");
                XLOG_ERROR(org_trans_tag(),
                           "Cache_write failed. {}. After_end_data {}",
                           err.message(), (flags_ & flags::tr_after_end_data));
                // Consume all available bytes, because we'll no longer
                // write to the cache after this.
                consume_cache_wr_data(*alive, org_cache_rdr_.bytes_avail());
                csm_->process_event(hhsm::ev_cache_op_err{alive.get()});
            }
            // TODO The code here is a hack, a bypass of the state machine
            // logic in order to avoid bloating it even more.
            // This is going to be fixed when we move the write stream
            // to the proxy connection - the T logic.
            // The above call will start blind tunnel if there has been
            // a pending blind tunnel. We must not call trans_completed
            // in this case.
            if (tr_completed && !in_blind_tunnel)
                csm_->process_event(hhsm::ev_trans_completed{alive.get()});
        });
}

void http_handler::consume_cache_wr_data(net::proxy_conn& conn,
                                         bytes32_t len) noexcept
{
    org_cache_rdr_.consume(len);
    // The reading from the origin may be blocked due to a lack of free
    // buffer. Inform it that it can resume receiving.
    // This is no-op if the receiving is not waiting.
    conn.wake_up_origin_recv();
}

void http_handler::cache_close() noexcept
{
    XLOG_DEBUG(org_trans_tag(), "Closing cache handle");
    cache_handle_.async_close();
}

////////////////////////////////////////////////////////////////////////////////

void http_handler::rem_bpctrl_entry() noexcept
{
    if (flags_ & flags::bpctrl_entry_added)
    {
        XLOG_INFO(no_trans_tag(), "BPCTRL rem entry");
        if (auto err = bp_ctrl_.add_del_entry(tag_.user_endpoint(),
                                              tag_.server_endpoint()))
        {
            XLOG_ERROR(no_trans_tag(), "BPCTRL error removing entry. {}",
                       err.message());
        }
        else
        {
            flags_ &= ~flags::bpctrl_entry_added;
            all_stats_.var_stats_.cnt_bpctrl_entries_ -= 1;
        }
    }
    else
    {
        XLOG_DEBUG(no_trans_tag(), "BPCTRL already removed");
    }
}

void http_handler::reset_per_trans_flags() noexcept
{
    flags_ &= ~(flags::tr_bpctrl_params_set | flags::tr_caching_started |
                flags::tr_after_end_data);
}

void http_handler::expand_client_recv_buff_if_needed(
    const http_trans& trans, net::proxy_conn& conn) noexcept
{
    auto set_new_rbuf_size = [&](net::proxy_conn& conn, auto idx)
    {
        client_rbuf_size_idx_ = idx;
        conn.expand_client_recv_buff(detail::client_rbuf_size[idx]);

    };
    switch (client_rbuf_size_idx_)
    {
    case detail::client_rbuf_size_def:
        if (const auto clen = trans.req_content_len())
        {
            if ((clen.value() > 512_KB))
                set_new_rbuf_size(conn, detail::client_rbuf_size_max);
            else if (clen.value() > 64_KB)
                set_new_rbuf_size(conn, detail::client_rbuf_size_mid);
        }
        break;
    case detail::client_rbuf_size_mid:
        if (const auto clen = trans.req_content_len())
        {
            if (clen.value() > 512_KB)
                set_new_rbuf_size(conn, detail::client_rbuf_size_max);
        }
        break;
    case detail::client_rbuf_size_max:
        // Don't need to (can't) expand the buffer
        break;
    }
}

void http_handler::expand_origin_recv_buff_if_needed(
    const http_trans& trans, net::proxy_conn& conn) noexcept
{
    switch (origin_rbuf_size_idx_)
    {
    case detail::origin_rbuf_size_def:
        if (const auto clen = trans.resp_content_len())
        {
            if (clen.value() > 512_KB)
            {
                const auto idx        = detail::origin_rbuf_size_max;
                origin_rbuf_size_idx_ = idx;
                conn.expand_origin_recv_buff(detail::origin_rbuf_size[idx]);
            }
        }
        break;
    case detail::origin_rbuf_size_max:
        // Don't need to (can't) expand the buffer
        break;
    }
}

const id_tag& http_handler::no_trans_tag() noexcept
{
    tag_.set_transaction_id(0);
    return tag_;
}

const id_tag& http_handler::cln_trans_tag() noexcept
{
    tag_.set_transaction_id(cln_trans_id_);
    return tag_;
}

const id_tag& http_handler::org_trans_tag() noexcept
{
    tag_.set_transaction_id(org_trans_id_);
    return tag_;
}

} // namespace detail
} // namespace http
