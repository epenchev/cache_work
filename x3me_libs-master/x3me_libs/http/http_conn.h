#pragma once

#include <memory>

#define BOOST_HTTP_SOCKET_DEFAULT_BUFFER_SIZE 1024
#include <boost/http/buffered_socket.hpp>
#undef BOOST_HTTP_SOCKET_DEFAULT_BUFFER_SIZE

#include "http_typedefs.h"
#include "http_req.h"
#include "http_status_code.h"

namespace x3me
{
namespace net
{

class http_resp;
class http_server_base;

class http_conn : public std::enable_shared_from_this<http_conn>
{
    boost::http::buffered_socket sock_;
    http_server_base& server_;
    http_req req_;

private:
    friend class http_server_base;
    struct private_tag
    {
    };

public:
    http_conn(io_service_t& ios, http_server_base& server, private_tag);
    ~http_conn();

    void async_write_resp(http_status_code code, const http_resp& resp);

private:
    tcp_socket_t& tcp_socket() { return sock_.next_layer(); }

    void async_recv_req();
    void check_recv_body();
};

} // namespace net
} // namespace x3me
