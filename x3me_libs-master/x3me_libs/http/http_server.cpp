#include "http_server.h"
#include "http_conn.h"
#include "http_req.h"
#include "http_resp.h"

namespace x3me
{
namespace net
{

http_server::http_server(io_service_t& ios) : http_server_base(ios)
{
}

http_server::~http_server()
{
}

void http_server::serve_req(http_req& req, const http_conn_ptr_t& conn)
{
    auto found = handlers_.find(req.path());
    if (found != handlers_.end())
    {
        found->second(req, conn);
        return;
    }
    found = handlers_.find("");
    if (found != handlers_.end())
    {
        found->second(req, conn);
        return;
    }
    conn->async_write_resp(http_status_code::not_found, http_resp{});
}

} // namespace net
} // namespace x3me
