#pragma once

namespace net
{

class proxy_conn;

class proto_handler
{
public:
    virtual ~proto_handler() noexcept {}

    virtual void init(proxy_conn&) noexcept = 0;

    virtual void on_origin_pre_connect(proxy_conn&) noexcept = 0;

    virtual void on_switched_stream_eof(proxy_conn&) noexcept = 0;

    virtual void on_client_data(proxy_conn&) noexcept = 0;
    virtual void on_client_recv_eof(proxy_conn&) noexcept = 0;
    virtual void on_client_recv_err(proxy_conn&) noexcept = 0;
    virtual void on_client_send_err(proxy_conn&) noexcept = 0;

    virtual void on_origin_data(proxy_conn&) noexcept = 0;
    virtual void on_origin_recv_eof(proxy_conn&) noexcept = 0;
    virtual void on_origin_recv_err(proxy_conn&) noexcept = 0;
    virtual void on_origin_send_err(proxy_conn&) noexcept = 0;
};

} // namespace net
