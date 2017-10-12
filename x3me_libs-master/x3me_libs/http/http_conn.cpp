#include "http_conn.h"
#include "http_server_base.h"
#include "http_resp.h"

#include <boost/utility/string_ref.hpp>

namespace x3me
{
namespace net
{

http_conn::http_conn(io_service_t& ios, http_server_base& server, private_tag)
    : sock_(ios), server_(server)
{
}

http_conn::~http_conn()
{
}

// TODO This whole connection functionality can become much more readable
// using ASIO coroutine.
void http_conn::async_write_resp(http_status_code code, const http_resp& resp)
{
    sock_.async_write_response(
        static_cast<uint_fast16_t>(code),
        boost::string_ref(http_status_code_msg(code)), resp.get(),
        [ c = shared_from_this(), resp ](const sys_error_t& err)
        {
            if (!err)
            {
                if (c->sock_.is_open())
                {
                    // This is a keep-alive connection
                    c->async_recv_req();
                }
            }
        });
}

void http_conn::async_recv_req()
{
    sock_.async_read_request(req_.method_, req_.path_, req_.msg_,
                             [c = shared_from_this()](const sys_error_t& err)
                             {
                                 if (!err)
                                 {
                                     // The request is fully received.
                                     // The library doesn't split the path from
                                     // the query
                                     // We need to do it manually.
                                     auto pos = c->req_.path_.find('?');
                                     if (pos != std::string::npos)
                                     {
                                         c->req_.query_ =
                                             c->req_.path_.substr(pos + 1);
                                         c->req_.path_.resize(pos);
                                     }
                                     c->check_recv_body();
                                 }
                             });
}

void http_conn::check_recv_body()
{
    switch (sock_.read_state())
    {
    case boost::http::read_state::message_ready: // Read the body
    {
        sock_.async_read_some(req_.msg_,
                              [c = shared_from_this()](const sys_error_t& err)
                              {
                                  if (!err)
                                      c->check_recv_body();
                              });
        return;
    }
    case boost::http::read_state::body_ready: // Read the trailers, if any
    {
        sock_.async_read_trailers(
            req_.msg_, [c = shared_from_this()](const sys_error_t& err)
            {
                if (!err)
                    c->check_recv_body();
            });
        return;
    }
    default: // empty or finished
        break;
    }
    server_.serve_req(req_, shared_from_this());
}

} // namespace net
} // namespace x3me
