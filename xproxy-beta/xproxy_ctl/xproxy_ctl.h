#pragma once

class settings;

class xproxy_ctl
{
    const settings& settings_;

public:
    explicit xproxy_ctl(const settings& sts) noexcept : settings_(sts) {}
    ~xproxy_ctl() noexcept = default;

    xproxy_ctl(const xproxy_ctl&) = delete;
    xproxy_ctl& operator=(const xproxy_ctl&) = delete;
    xproxy_ctl(xproxy_ctl&&) = delete;
    xproxy_ctl& operator=(xproxy_ctl&&) = delete;

    bool exec(string_view_t cmd) noexcept;

private:
    bool verify_debug_cmd(string_view_t filter) noexcept;
    bool exec_json_rpc(string_view_t method, string_view_t param) noexcept;
};
