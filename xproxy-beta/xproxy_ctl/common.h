#pragma once

using string_view_t = boost::string_view;
using ip_addr4_t    = boost::asio::ip::address_v4;
using err_code_t    = boost::system::error_code;

////////////////////////////////////////////////////////////////////////////////

constexpr string_view_t operator""_sv(char const* str, size_t len) noexcept
{
    return {str, len};
}
