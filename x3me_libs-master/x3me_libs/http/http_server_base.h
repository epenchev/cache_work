#pragma once

#include <memory>

#include "http_typedefs.h"

namespace x3me
{
namespace net
{

class http_req;
class http_conn;
using http_conn_ptr_t = std::shared_ptr<http_conn>;

class http_server_base
{
    tcp_acceptor_t acceptor_;

public:
    explicit http_server_base(io_service_t& ios);
    virtual ~http_server_base();

    http_server_base() = delete;
    http_server_base(const http_server_base&) = delete;
    http_server_base& operator=(const http_server_base&) = delete;
    http_server_base(http_server_base&&) = delete;
    http_server_base& operator=(http_server_base&&) = delete;

    bool start(const ip_addr_t& addr, uint16_t port, sys_error_t& err);
    void stop();

private:
    bool setup_acceptor(const ip_addr_t& addr, uint16_t port, sys_error_t& err);
    void accept_conn();

private:
    friend class http_conn;
    virtual void serve_req(http_req& req, const http_conn_ptr_t& conn) = 0;
};

} // namespace net
} // namespace x3me
