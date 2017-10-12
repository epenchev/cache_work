#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace x3me
{
namespace net
{

using io_service_t   = boost::asio::io_service;
using tcp_acceptor_t = boost::asio::ip::tcp::acceptor;
using tcp_socket_t   = boost::asio::ip::tcp::socket;
using ip_addr_t      = boost::asio::ip::address;
using tcp_endpoint_t = boost::asio::ip::tcp::endpoint;
using sys_error_t    = boost::system::error_code;

} // namespace net
} // namespace x3me
