#include <algorithm>

#include "json_rpc_server.h"
#include "../http/http_conn.h"
#include "../http/http_req.h"
#include "../http/http_resp.h"

namespace x3me
{
namespace net
{
namespace json_rpc
{

void set_content_type(http_resp& resp, const char* type)
{
    auto& hdrs = resp.headers();
    hdrs.erase("content-type"); // The library works with small caps only
    hdrs.emplace("content-type", type);
}

////////////////////////////////////////////////////////////////////////////////

server::server(io_service_t& ios) : http_server_base(ios)
{
}

server::~server()
{
}

void server::serve_req(http_req& req, const http_conn_ptr_t& conn)
{
    if (req.method() != "POST")
    {
        const char msg[] = "Only POST method is supported";
        http_resp resp;
        resp.body().reserve(sizeof(msg) - 1);
        std::copy(std::begin(msg), std::end(msg),
                  std::back_inserter(resp.body()));
        set_content_type(resp, "text/plain");
        conn->async_write_resp(http_status_code::method_not_allowed, resp);
        return;
    }

    const auto& hdrs = req.headers();
    auto it = hdrs.find("content-type");
    if ((it == hdrs.end()) ||
        ((strcasecmp(it->second.c_str(), "application/json-rpc") != 0) &&
         (strcasecmp(it->second.c_str(), "application/json") != 0) &&
         (strcasecmp(it->second.c_str(), "application/jsonrequest") != 0)))
    {
        const char msg[] = "Missing or invalid 'Content-Type'. Expected "
                           "'application/json-rpc' or 'application/json' or "
                           "'application/jsonrequest'";
        http_resp resp;
        resp.body().reserve(sizeof(msg) - 1);
        std::copy(std::begin(msg), std::end(msg),
                  std::back_inserter(resp.body()));
        set_content_type(resp, "text/plain");
        conn->async_write_resp(http_status_code::bad_request, resp);
        return;
    }

    handler_.process(req.body(), conn);
}

} // namespace json_rpc
} // namespace net
} // namespace x3me
