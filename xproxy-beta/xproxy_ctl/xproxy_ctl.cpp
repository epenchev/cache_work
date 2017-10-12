#include "precompiled.h"
#include "xproxy_ctl.h"
#include "settings.h"
#include "debug_cmd.h"
#include "debug_cmd_printer.h"
#include "json_rpc.h"
#include "../debug_filter.h"

bool xproxy_ctl::exec(string_view_t full_cmd) noexcept
{
    using boost::algorithm::starts_with;

    constexpr auto debug_on   = "debug on"_sv;
    constexpr auto debug_off  = "debug off"_sv;
    constexpr auto debug_on_  = "debug on "_sv;
    constexpr auto debug_off_ = "debug off "_sv;

    constexpr auto method_debug_on  = "debug_on"_sv;
    constexpr auto method_debug_off = "debug_off"_sv;

    if ((full_cmd == debug_on) || starts_with(full_cmd, debug_on_))
    {
        const auto filt =
            full_cmd.substr(std::min(debug_on_.size(), full_cmd.size()));
        return verify_debug_cmd(filt) && exec_json_rpc(method_debug_on, filt);
    }
    else if ((full_cmd == debug_off) || starts_with(full_cmd, debug_off_))
    {
        const auto filt =
            full_cmd.substr(std::min(debug_off_.size(), full_cmd.size()));
        return verify_debug_cmd(filt) && exec_json_rpc(method_debug_off, filt);
    }

    LOG_ERRR("Unrecognized cmd: '" << full_cmd << '\'');
    return false;
}

bool xproxy_ctl::verify_debug_cmd(string_view_t filter) noexcept
{
    if (filter.empty())
    {
        LOG_DEBG("Debug filter empty. It's OK");
        return true;
    }

    LOG_DEBG("Verifying debug filter: '" << filter << '\'');

    bool res = false;
    auto it  = filter.cbegin();
    auto end = filter.cend();

    dcmd::ast::group_expr cmd_tree;
    boost::spirit::x3::ascii::space_type space; // Skip additional spaces

    if (!debug_log::enabled())
    {
        res = phrase_parse(it, end, dcmd::cmd_descr, space);
    }
    else
    {
        res = phrase_parse(it, end, dcmd::cmd_descr, space, cmd_tree);
    }

    res = res && (it == end);

    if (res)
    {
        LOG_DEBG("Valid debug filter!");
        if (debug_log::enabled())
        {
            dcmd::printer()(cmd_tree);
            std::cout << '\n';
        }
    }
    else
    {
        LOG_ERRR("Invalid debug filter. Verify stopped at: '"
                 << boost::string_view(it, end - it) << '\'');
    }

    return res;
}

bool xproxy_ctl::exec_json_rpc(boost::string_view method,
                               boost::string_view param) noexcept
{
    using namespace json_rpc;

    x3me::utils::string_builder_64 url_strm;
    url_strm << "http://" << settings_.mgmt_bind_ip() << ':'
             << settings_.mgmt_bind_port();
    const auto url = url_strm.to_string();

    LOG_DEBG("Sending debug cmd to " << url);

    using namespace urdl::http;
    urdl::istream is;
    is.set_option(request_method("POST"));
    is.set_option(request_content_type("application/json"));
    is.set_option(request_content(prepare_json_rpc_req(method, param)));

    is.open(url);
    if (is.error())
    {
        LOG_ERRR("JSON-RPC method: '" << method << "'\nParam: '" << param
                                      << "'\nFailure: Communication error with "
                                      << url << ". " << is.error().message());
        return false;
    }

    std::string resp;
    while (is)
    {
        char buffer[512];
        is.read(buffer, sizeof(buffer));
        resp.append(buffer, is.gcount());
    }

    resp = get_json_rpc_resp(resp);

    LOG_INFO("JSON-RPC method: '" << method << "'\nParam: '" << param
                                  << "'\nResult: " << resp);

    return true;
}
