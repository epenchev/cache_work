#include "precompiled.h"
#include "xproxy.h"
#include "settings.h"
#include "cache/cache_stats.h"
#include "http/handler_factory.h"
#include "http/http_constants.h"
#include "net/proxy_conn.h"
#include "xutils/ref_counted.h"
#include "xutils/sys_funcs.h"

xproxy::xproxy(const settings& sts) noexcept
    : settings_(sts),
      acceptor_(main_ios_),
      signal_set_(main_ios_, SIGINT, SIGTERM),
      net_workers_(sts.main_scale_factor() *
                   std::thread::hardware_concurrency()),
      curr_net_worker_(0),
      mgmt_server_(sts)
{
    XLOG_INFO(main_tag, "XProxy constructed. Net workers {}",
              net_workers_.size());
    // The hardware_concurrency is allowed to return 0 in some undefined
    // cases. I don't expect to happen in practice though.
    X3ME_ENFORCE(!net_workers_.empty());
    net::proxy_conn::half_closed.resize(net_workers_.size());
}

xproxy::~xproxy() noexcept
{
    XLOG_INFO(main_tag, "XProxy destructed");
}

bool xproxy::run(bool reset_cache, bool fast_exit) noexcept
{
    bool res;
    if (!reset_cache)
    {
        if ((res = init()))
            run_impl(fast_exit);
    }
    else
    {
        res = cache_mgr_.reset_volumes(settings_);
    }
    return res;
}

bool xproxy::init() noexcept
{
    err_code_t err;
    if (!xutils::set_max_count_fds(settings_.main_max_count_fds(), err))
    {
        XLOG_FATAL(main_tag,
                   "Unable to set the max count of opened files to {}. {}",
                   settings_.main_max_count_fds(), err.message());
        return false;
    }

    if (!relinquish_privileges())
        return false;

    if (!init_plugins_mgr())
        return false;

    if (!setup_acceptor())
        return false;

    if (!init_mgmt_server())
        return false;

    if (!cache_mgr_.start(settings_))
    {
        mgmt_server_.stop();
        return false;
    }

    set_subsystems_settings();

    // Note that the checks for the half closed are scheduled from the
    // main thread here, before the worker threads are started.
    // Later when the threads are started the scheduling is done from the
    // corresponding thread.
    for (size_t i = 0; i < net_workers_.size(); ++i)
        schedule_check_half_closed(i);

    handle_sys_signal();

    accept_connection();

    return true;
}

void xproxy::run_impl(bool fast_exit) noexcept
{
    using x3me::sys_utils::set_this_thread_name;

    std::vector<std::thread> net_threads;
    net_threads.reserve(net_workers_.size());
    std::atomic_uint cnt_failed(0);
    boost::latch latch(net_workers_.size());

    for (auto& s : net_workers_)
    {
        net_threads.emplace_back(
            [&s, &latch, &cnt_failed]
            {
                set_this_thread_name("xproxy_net");
                if (auto err = s.bp_ctrl_.init())
                {
                    XLOG_FATAL(main_tag, "Unable to init back pressure control "
                                         "module. {}",
                               err.message());
                    ++cnt_failed;
                    latch.count_down();
                }
                else
                {
                    latch.count_down();
                    s.ios_.run();
                }
            });
    }

    latch.wait();

    if (cnt_failed == 0)
    {
        set_this_thread_name("xproxy_main");

        XLOG_INFO(main_tag, "XProxy fully functional");

        // Run can throw if we call it again after being stopped,
        // but we are not in this case. So, let's simplify the code.
        main_ios_.run();

        XLOG_INFO(main_tag, "XProxy stopped");
    }
    else
    {
        XLOG_FATAL(main_tag, "Unable to init back pressure control "
                             "module on some net threads. Please, check if the "
                             "kernel module is installed");
    }

    mgmt_server_.stop();

    // Stop the workers
    for (auto& s : net_workers_)
        s.ios_.stop();
    // Wait for them to exit
    for (auto& t : net_threads)
    {
        if (t.joinable())
            t.join();
    }

    // We can now stop the cache functionality. We know that there will be
    // no network activity to issue tasks for the cache system.
    cache_mgr_.stop();
    // After the cache is stopped we can exit safely using the fastest
    // possible way. However, this may hide some bugs on
    // exit and will hide also potential memory leaks (or will show a lot
    // of memory leaks in tools like valgrind).
    if (fast_exit)
        ::_Exit(EXIT_SUCCESS);

    // Explicitly destroy all workers and their async objects
    // to avoid potential ordering related crashes in the destructor.
    net_workers_.clear();
}

bool xproxy::init_plugins_mgr() noexcept
{
    // Use the io_service of the managment/statistics server because it's
    // mostly idle anyway.
    const net_thread_id_t cnt_net_threads = net_workers_.size();
    return plugins_mgr_.init(
        settings_,
        plgns::net_thread_exec{
            cnt_net_threads,
            [this](net_thread_id_t tid, const std::function<void()>& fn)
            {
                X3ME_ASSERT(tid < net_workers_.size(), "Invalid net thread id");
                net_workers_[tid].ios_.post(fn);
            }});
}

bool xproxy::init_mgmt_server() noexcept
{
    mgmt_server_.fn_summary_net_stats =
        make_mem_fn_delegate(&xproxy::mgmt_cb_summary_net_stats, this);
    mgmt_server_.fn_summary_http_stats =
        make_mem_fn_delegate(&xproxy::mgmt_cb_summary_http_stats, this);
    mgmt_server_.fn_resp_size_http_stats =
        make_mem_fn_delegate(&xproxy::mgmt_cb_resp_size_http_stats, this);
    mgmt_server_.fn_cache_stats =
        make_mem_fn_delegate(&xproxy::mgmt_cb_cache_stats, this);
    mgmt_server_.fn_cache_internal_stats =
        make_mem_fn_delegate(&xproxy::mgmt_cb_cache_internal_stats, this);
    return mgmt_server_.start(settings_.mgmt_bind_ip(),
                              settings_.mgmt_bind_port());
}

bool xproxy::relinquish_privileges() noexcept
{
    const auto user     = settings_.priv_user();
    const auto work_dir = settings_.priv_work_dir();

    auto pw_rsize = []() -> uint32_t
    {
        const auto r = sysconf(_SC_GETPW_R_SIZE_MAX);
        return r > 0 ? r : 4_KB;
    };

    namespace bsys = boost::system;
    try
    {
        const auto len = pw_rsize();
        auto buf = static_cast<char*>(::malloc(len));
        X3ME_SCOPE_EXIT { ::free(buf); };

        passwd pwd;
        passwd* res    = nullptr;
        const auto err = ::getpwnam_r(user.c_str(), &pwd, buf, len, &res);
        if (!res)
        {
            if (err == 0)
                throw std::invalid_argument("User not found");
            else
                throw bsys::system_error(err, bsys::get_system_category(),
                                         "Unable to obtain user information");
        }
        // Preserve the capabilities
        if (::prctl(PR_SET_KEEPCAPS, 1) != 0)
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to preserve capabilities");
        }
        // First relinquish the privileges to the logs and working directory.
        // The solution has negatives, because if some of the operations
        // after that fails the directories/files will remain with the changed
        // privileges, but it's not that bad (IMO) and shouldn't actually
        // happen in practice.
        chown_logs(settings_.log_logs_dir(), pwd.pw_uid, pwd.pw_gid);
        if (::chown(work_dir.c_str(), pwd.pw_uid, pwd.pw_gid) != 0)
        {
            throw bsys::system_error(
                errno, bsys::get_system_category(),
                "Unable to change owner of the work directory");
        }
        // Second change the working directory
        if (::chdir(work_dir.c_str()) != 0)
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to change the working dir");
        }
        // Third repopulate the supplementary group list for the user
        if (::initgroups(pwd.pw_name, pwd.pw_gid) != 0)
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to repopulate user groups");
        }
        // Forth relinquish the privileges in the correct order
        if (::setregid(pwd.pw_gid, pwd.pw_gid) != 0)
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to change the group id");
        }
        if (::setreuid(pwd.pw_uid, pwd.pw_uid) != 0)
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to change the user id");
        }
        // Last preserve only the needed capabilities
        auto set_needed_capabilities = []
        {
            cap_t caps = ::cap_init(); // start with nothing.
            // CAP_NET_ADMIN capability is needed for setting socket options
            // such as TPROXY.
            // CAP_NET_BIND_SERVICE is needed so that we can bind on ports
            // lower than 1024.
            // The ATS initially sets only CAP_NET_ADMIN, CAP_NET_BIND_SERVICE
            // as effective capabilities but later adds CAP_DAC_OVERRIDE
            // in order to be able to read/write raw devices.
            std::array<cap_value_t, 3> lst = {
                CAP_NET_ADMIN, CAP_NET_BIND_SERVICE, CAP_DAC_OVERRIDE};
            cap_set_flag(caps, CAP_PERMITTED, lst.size(), lst.data(), CAP_SET);
            cap_set_flag(caps, CAP_EFFECTIVE, lst.size(), lst.data(), CAP_SET);
            const auto res = ::cap_set_proc(caps);
            ::cap_free(caps);
            return (res == 0);
        };
        if (!set_needed_capabilities())
        {
            throw bsys::system_error(errno, bsys::get_system_category(),
                                     "Unable to set needed capabilities");
        }
    }
    catch (const bsys::system_error& err)
    {
        XLOG_FATAL(main_tag, "Unable to relinquish process privileges. {}. {}. "
                             "User: {}. Work_dir: {}",
                   err.what(), err.code().message(), user, work_dir);
        return false;
    }
    catch (const std::exception& ex)
    {
        XLOG_FATAL(main_tag, "Unable to relinquish process privileges. {}. "
                             "User: {}. Work_dir: {}",
                   ex.what(), user, work_dir);
        return false;
    }
    if ((::getuid() == 0) || (::geteuid() == 0))
    {
        XLOG_FATAL(
            main_tag,
            "Xproxy is not designed to run as root. Please change the user!");
        return false;
    }
    return true;
}

bool xproxy::setup_acceptor() noexcept
{
    const tcp_endpoint_t bind_ep(settings_.main_bind_ip(),
                                 settings_.main_bind_port());
    try
    {
        using boost::asio::socket_base;
        acceptor_.open(bind_ep.protocol());
        acceptor_.set_option(socket_base::reuse_address(true));
        acceptor_.set_option(x3me_sockopt::transparent_mode(true));
        acceptor_.bind(bind_ep);
        // The min value of SOMAXCONN and /proc/sys/net/core/somaxconn
        acceptor_.listen(socket_base::max_connections);
    }
    catch (const std::exception& ex)
    {
        XLOG_FATAL(main_tag, "Unable to setup the TCP acceptor on {}. {}",
                   bind_ep, ex.what());
        return false;
    }
    return true;
}

void xproxy::accept_connection() noexcept
{
    // We don't want atomic reference counting for the sockets here, because
    // they are used/passed always to a single thread.
    auto sock = xutils::make_ref_counted<tcp_socket_t>(main_ios_);

    acceptor_.async_accept(
        sock->data_, [sock, this](const err_code_t& err)
        {
            static uint64_t print_cnt = 0;
            if (!err)
            {
                print_cnt = 0;
                distribute_connection(std::move(sock->data_));
                accept_connection(); // Accept new one
            }
            else if (err == boost::system::errc::too_many_files_open)
            {
                // Don't log this message all the time if we constantly
                // hit the limit.
                if ((print_cnt % 25) == 0)
                {
                    XLOG_ERROR(
                        main_tag,
                        "'open files' limit is reached. May eat up the CPUs");
                }
                // This is going to eat 100% CPU if the limit is constantly
                // reached
                accept_connection();
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_WARN(main_tag, "Accept connection error. {}",
                          err.message());
                accept_connection();
            }
        });
}

#ifdef X3ME_APP_TEST
extern tcp_endpoint_t g_server_ep;
#endif

void xproxy::distribute_connection(tcp_socket_t&& sock) noexcept
{
    const int sock_fd = ::dup(sock.native_handle());
    if (!(sock_fd > 0))
    {
        // The error may happen if for example a connection is opened
        // and closed while being processed and thus the duplicated
        // socket is no longer valid. There could be other reasons too.
        err_code_t err(errno, boost::system::system_category());
        XLOG_ERROR(main_tag, "Unable to duplicate socket. ", err.message());
        return;
    }

    const auto sess_id            = curr_session_id_++;
    auto& wrk                     = net_workers_[curr_net_worker_];
    const net_thread_id_t net_idx = curr_net_worker_;
    auto cache_mgr                = &cache_mgr_;
    // A bit dangerous post of naked fd, if the asio has post bug
    // and the handler never gets executed :). However, if there is such
    // an asio bug we'll have much bigger problems than sockets leakage.
    wrk.ios_.post(
        [sock_fd, sess_id, &wrk, net_idx, cache_mgr]
        {
            try
            {
                using namespace boost::asio;
                tcp_socket_t client_sock(wrk.ios_, ip::tcp::v4(), sock_fd);

                auto tag = net_tag;
                tag.set_session_id(sess_id);
#ifndef X3ME_APP_TEST
                // Some of these may throw
                tag.set_user_endpoint(client_sock.remote_endpoint());
                tag.set_server_endpoint(client_sock.local_endpoint());
#else
                // We need to bind to the server IP if we want the test
                // scheme to work. At least we use the client port :).
                auto ep = client_sock.local_endpoint();
                ep.port(client_sock.remote_endpoint().port());
                tag.set_user_endpoint(ep);
                tag.set_server_endpoint(g_server_ep);
#endif // X3ME_APP_TEST
                auto conn = net::make_proxy_conn(
                    tag, std::move(client_sock), http::client_rbuf_block_size,
                    http::origin_rbuf_block_size, wrk.stats_.net_stats_,
                    net_idx);
                conn->start(http::make_handler_factory(
                    *cache_mgr, wrk.stats_.http_stats_, wrk.bp_ctrl_));
            }
            catch (const boost::system::system_error& err)
            {
                // This fails because of already disconnected transport
                // endpoint in 99% of the cases.
                XLOG_INFO(main_tag,
                          "Error when initializing proxy connection. {}",
                          err.what());
            }
        });
    curr_net_worker_ = (curr_net_worker_ + 1) % net_workers_.size();
    if (curr_session_id_ == 0) // Overflow
        curr_session_id_ = 1; // Better for non-programmers to start from 1, IMO
}

void xproxy::schedule_check_half_closed(net_thread_id_t net_tid) noexcept
{
    auto& tmr = net_workers_[net_tid].half_closed_tmr_;
    err_code_t err;
    tmr.expires_from_now(std::chrono::seconds(60), err);
    if (err)
    {
        XLOG_FATAL(main_tag, "Half closed connections timer setup failure. {}",
                   err.message());
        std::cerr << "Half closed connections timer setup failure. "
                  << err.message() << std::endl;
        ::abort();
    }
    tmr.async_wait(
        [this, net_tid](const err_code_t& err)
        {
            if (!err)
            {
                // Note that the timer is executed in the context of
                // the given io_service thread.
                check_half_closed(net_tid);
                schedule_check_half_closed(net_tid);
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_FATAL(main_tag,
                           "Half closed connections timer failure. {}",
                           err.message());
                std::cerr << "Half closed connections timer failure. "
                          << err.message() << std::endl;
                ::abort();
            }
        });
}

void xproxy::check_half_closed(net_thread_id_t net_tid) noexcept
{
    size_t closed   = 0;
    auto& conn_list = net::proxy_conn::half_closed[net_tid];
    for (auto& c : conn_list)
    {
        // It's important that the close operation doesn't act as delete this
        // otherwise we'll be removing from this list while iterating and
        // we are going to have problems (UB).
        closed += c.close_if_stalled();
    }
    XLOG_DEBUG(
        main_tag,
        "Worker_idx {}. Checked {} half closed connections. Found {} stalled",
        net_tid, conn_list.size(), closed);
    net_workers_[net_tid].stats_.net_stats_.cnt_closed_half_closed_ += closed;
}

void xproxy::handle_sys_signal() noexcept
{
    signal_set_.async_wait(
        [this](const err_code_t& err, int sig_num)
        {
            if (!err)
            {
                switch (sig_num)
                {
                case SIGINT:
                    XLOG_INFO(main_tag, "SIGINT received");
                    main_ios_.stop();
                    break;
                case SIGTERM:
                    XLOG_INFO(main_tag, "SIGTERM received");
                    main_ios_.stop();
                    break;
                default:
                    // Must not happen in practice.
                    // Means that we have subscribed for
                    // a signal but don't handle it here.
                    XLOG_ERROR(main_tag, "Received unexpected signal {}. "
                                         "Aborting the application",
                               sig_num);
                    abort();
                    break;
                }
            }
            else if (err != asio_error::operation_aborted)
            {
                XLOG_ERROR(main_tag, "Can't handle system signals. {}",
                           err.message());
            }
        });
}

void xproxy::set_subsystems_settings() noexcept
{
    net::proxy_conn::tos_mark_hit  = (settings_.main_dscp_hit() << 2);
    net::proxy_conn::tos_mark_miss = (settings_.main_dscp_miss() << 2);

    http::constants::bpctrl_window_size = settings_.main_kmod_def_window();
}

////////////////////////////////////////////////////////////////////////////////
namespace
{
template <typename Stats, typename Callback>
struct stats_helper
    : boost::intrusive_ref_counter<stats_helper<Stats, Callback>,
                                   boost::thread_safe_counter>
{
    std::vector<Stats> stats_;
    Callback cb_;

    stats_helper(const Callback& cb, size_t size) : stats_(size), cb_(cb) {}

    // Ensures that the last worker collecting stats will call the callback.
    ~stats_helper()
    {
        // Sum statistics from every thread. The stats have only operator+=,
        // because it could be implemented in a light way. Thus we can't use
        // std::accumulate which needs operator+.
        Stats sum_stats;
        for (const auto& s : stats_)
            sum_stats += s;
        cb_(std::move(sum_stats));
    }
};

template <typename Stats, typename Callback>
auto make_stats_helper(const Callback& cb, size_t size) noexcept
{
    using sh_t = stats_helper<Stats, Callback>;
    return boost::intrusive_ptr<sh_t>(new sh_t(cb, size));
}
} // namespace

void xproxy::mgmt_cb_summary_net_stats(
    const mgmt::summary_net_stats_cb_t& cb) noexcept
{
    auto sh = make_stats_helper<net::all_stats>(cb, net_workers_.size());
    // Collect the stats from the corresponding thread
    for (size_t i = 0; i < net_workers_.size(); ++i)
    {
        auto& w         = net_workers_[i];
        const auto& sts = w.stats_.net_stats_;
        w.ios_.post([sh, &sts, i]
                    {
                        auto& ss = sh->stats_[i];
                        ss = sts;
                        ss.cnt_curr_half_closed_ =
                            net::proxy_conn::half_closed[i].size();
                    });
    }
}

void xproxy::mgmt_cb_summary_http_stats(
    const mgmt::summary_http_stats_cb_t& cb) noexcept
{
    auto sh = make_stats_helper<http::var_stats>(cb, net_workers_.size());
    // Collect the stats from the corresponding thread
    for (size_t i = 0; i < net_workers_.size(); ++i)
    {
        auto& w         = net_workers_[i];
        const auto& sts = w.stats_.http_stats_.var_stats_;
        w.ios_.post([sh, &sts, i]
                    {
                        sh->stats_[i] = sts;
                    });
    }
}

void xproxy::mgmt_cb_resp_size_http_stats(
    const mgmt::resp_size_http_stats_cb_t& cb) noexcept
{
    auto sh = make_stats_helper<http::resp_size_stats>(cb, net_workers_.size());
    // Collect the stats from the corresponding thread
    for (size_t i = 0; i < net_workers_.size(); ++i)
    {
        auto& w         = net_workers_[i];
        const auto& sts = w.stats_.http_stats_.resp_size_stats_;
        w.ios_.post([sh, &sts, i]
                    {
                        sh->stats_[i] = sts;
                    });
    }
}

void xproxy::mgmt_cb_cache_stats(const mgmt::cache_stats_cb_t& cb) noexcept
{
    // Call it directly in the management thread.
    // The call to the cache_mgr method is thread safe.
    cb(cache_mgr_.get_stats());
}

void xproxy::mgmt_cb_cache_internal_stats(
    const mgmt::cache_internal_stats_cb_t& cb) noexcept
{
    // Call it directly in the management thread.
    // The call to the cache_mgr method is thread safe.
    cb(cache_mgr_.get_internal_stats());
}
