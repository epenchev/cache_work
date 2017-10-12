#pragma once

#include <functional>

#include <boost/container/flat_map.hpp>

#include "http_typedefs.h"
#include "http_server_base.h"

namespace x3me
{
namespace net
{

class http_server final : public http_server_base
{
    using handler_t =
        std::function<void(const http_req&, const http_conn_ptr_t&)>;
    using handlers_t = boost::container::flat_map<std::string, handler_t>;

    handlers_t handlers_;

public:
    explicit http_server(io_service_t& ios);
    ~http_server();

    http_server() = delete;
    http_server(const http_server&) = delete;
    http_server& operator=(const http_server&) = delete;
    http_server(http_server&&) = delete;
    http_server& operator=(http_server&&) = delete;

    template <typename Str, typename Handler>
    void on_request(Str&& path, Handler&& handler)
    {
        handlers_.emplace(std::forward<Str>(path),
                          std::forward<Handler>(handler));
    }
    template <typename Handler>
    void on_request(Handler&& handler)
    {
        handlers_.emplace("", std::forward<Handler>(handler));
    }

private:
    void serve_req(http_req& req, const http_conn_ptr_t& conn) final;
};

} // namespace net
} // namespace x3me
