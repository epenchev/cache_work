#pragma once

namespace http
{
class http_trans;
} // namespace http

namespace plgns
{

struct plugins
{
    static plugins* instance;

    virtual ~plugins() noexcept {}

    virtual void on_before_cache_open_read(net_thread_id_t,
                                           http::http_trans&) noexcept = 0;
    virtual void on_transaction_end(net_thread_id_t,
                                    http::http_trans&) noexcept = 0;
};

} // namespace plgns
