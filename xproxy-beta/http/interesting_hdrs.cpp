#include "precompiled.h"
#include "interesting_hdrs.h"

namespace http
{
namespace detail
{

constexpr const_string_t intr_req_hdrs::hdrs_[];
constexpr const_string_t intr_req_hdrs::unsupported_hdrs_[];

constexpr const_string_t intr_resp_hdrs::hdrs_[];
constexpr const_string_t intr_resp_hdrs::unsupported_hdrs_[];

req_hdr req_hdr_idx(const string_view_t& hdr) noexcept
{
    size_t i = 0;
    for (const auto& h : intr_req_hdrs::hdrs_)
    {
        if ((h.size() == hdr.size()) &&
            (::strncasecmp(h.data(), hdr.data(), h.size()) == 0))
        {
            return static_cast<req_hdr>(i);
        }
        ++i;
    }
    return req_hdr::unknown;
}

resp_hdr resp_hdr_idx(const string_view_t& hdr) noexcept
{
    size_t i = 0;
    for (const auto& h : intr_resp_hdrs::hdrs_)
    {
        if ((h.size() == hdr.size()) &&
            (::strncasecmp(h.data(), hdr.data(), h.size()) == 0))
        {
            return static_cast<resp_hdr>(i);
        }
        ++i;
    }
    return resp_hdr::unknown;
}

} // namespace detail
} // namespace http
