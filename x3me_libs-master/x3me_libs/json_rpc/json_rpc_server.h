#pragma once

#include <functional>

#include "../http/http_server_base.h"
#include "json_rpc_handler.h"

namespace x3me
{
namespace net
{
namespace json_rpc
{

class server final : public http_server_base
{
    detail::handler handler_;

private:
    template <typename T>
    struct deduce_type;
    template <typename C, typename... Args>
    struct deduce_type<void (C::*)(Args...) const>
    {
        using fn_type = std::function<void(Args&...)>;
    };

    // Make callback from std::function. Just return it.
    template <typename... Args>
    static auto make_cb(const std::function<void(Args&...)>& fn)
    {
        return fn;
    }

    // Make callback from Functor/Lambda deducing the arguments type.
    template <typename Fn, typename = decltype(&Fn::operator())>
    static auto make_cb(Fn&& fn)
    {
        using fn_type =
            typename deduce_type<decltype(&Fn::operator())>::fn_type;
        return fn_type(std::forward<Fn>(fn));
    }

public:
    explicit server(io_service_t& ios);
    ~server();

    server() = delete;
    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;

    // Accepts std::function objects or Functors/Lambdas
    template <typename Callback>
    bool add_callback(const char* name, Callback&& cb)
    {
        return handler_.add_callback(name, make_cb(std::forward<Callback>(cb)));
    }

private:
    void serve_req(http_req& req, const http_conn_ptr_t& conn) final;
};

} // namespace json_rpc
} // namespace net
} // namespace x3me
