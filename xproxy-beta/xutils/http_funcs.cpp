#include "precompiled.h"
#include "http_funcs.h"

namespace xutils
{

string_view_t get_host(const string_view_t& url) noexcept
{
    constexpr string_view_t http{"http://", 7};
    // Remove the protocol
    const auto url_sv =
        boost::istarts_with(url, http)
            ? string_view_t{url.data() + http.size(), url.size() - http.size()}
            : string_view_t{url.data(), url.size()};
    // Trim up to the found port, path or query string, whichever is first.
    constexpr string_view_t host_end{":/?", 3};
    const auto pos = url_sv.find_first_of(host_end);
    return (pos != string_view_t::npos) ? url_sv.substr(0, pos) : url_sv;
}

string_view_t truncate_host(const string_view_t& host,
                            uint16_t domain_lvl) noexcept
{
    auto rbeg = host.rbegin();
    auto rend = host.rend();
    if ((domain_lvl != 0) && (rbeg != rend) &&
        !::isdigit(*rbeg)) // Don't parse dottend ips with this logic
    {
        uint16_t dots = 0;

        rend = std::find_if(rbeg, rend, [&](char ch)
                            {
                                return (ch == '.') && (++dots == domain_lvl);
                            });
    }
    const auto beg = rend.base();
    const auto end = rbeg.base();
    return {&(*beg), static_cast<size_t>(end - beg)};
}

} // namespace xutils
