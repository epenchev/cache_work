#include <iostream>

#include "http_server_base.h"
#include "http_conn.h"

namespace x3me
{
namespace net
{

http_server_base::http_server_base(io_service_t& ios) : acceptor_(ios)
{
}

http_server_base::~http_server_base()
{
}

bool http_server_base::start(const ip_addr_t& addr, uint16_t port,
                             sys_error_t& err)
{
    bool res = setup_acceptor(addr, port, err);
    if (res)
        accept_conn();
    return res;
}

void http_server_base::stop()
{
    sys_error_t ignored;
    acceptor_.close(ignored);
}

bool http_server_base::setup_acceptor(const ip_addr_t& addr, uint16_t port,
                                      sys_error_t& err)
{
    try
    {
        const auto bind_ep = tcp_endpoint_t(addr, port);
        acceptor_.open(bind_ep.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(bind_ep);
        acceptor_.listen(boost::asio::socket_base::max_connections);
    }
    catch (const boost::system::system_error& ex)
    {
        err = ex.code();
        return false;
    }
    return true;
}

void http_server_base::accept_conn()
{
    auto conn = std::make_shared<http_conn>(acceptor_.get_io_service(), *this,
                                            http_conn::private_tag{});
    acceptor_.async_accept(
        conn->tcp_socket(), [=](const sys_error_t& err)
        {
            if (!err)
            {
                conn->async_recv_req();
                accept_conn();
            }
            else if (err == boost::system::errc::too_many_files_open)
            {
                // TODO
                std::cerr << "http_server_base accept error. " << err.message()
                          << std::endl;
                accept_conn();
            }
            else if (err != boost::asio::error::operation_aborted)
            {
                // TODO
                std::cerr << "http_server_base accept error. " << err.message()
                          << std::endl;
            }
        });
}

} // namespace net
} // namespace x3me
