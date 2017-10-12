#pragma once

namespace http
{
namespace detail
{

#define REQ_HDRS(XX)                                                           \
    XX(content_length, "Content-Length")                                       \
    XX(host, "Host")

enum struct req_hdr
{
#define XX(name, str) name,
    REQ_HDRS(XX)
#undef XX
        unknown,
};

struct intr_req_hdrs
{
    // We want to inspect the value of such headers
    // clang-format off
    static constexpr const_string_t hdrs_[] =
    {
#define XX(name, str) str,
        REQ_HDRS(XX)
#undef XX
    };
    // We need to go to blind tunnel mode if such headers are present.
    static constexpr const_string_t unsupported_hdrs_[] =
    {
        "Upgrade",
        "Authorization",
    };
    // clang-format on
};

req_hdr req_hdr_idx(const string_view_t& hdr) noexcept;

inline constexpr const const_string_t& hdr_str(req_hdr h) noexcept
{
    assert(h != req_hdr::unknown);
    return intr_req_hdrs::hdrs_[static_cast<std::underlying_type_t<req_hdr>>(
        h)];
}

#undef REQ_HDRS

////////////////////////////////////////////////////////////////////////////////

#define RESP_HDRS(XX)                                                          \
    XX(cache_control, "Cache-Control")                                         \
    XX(content_encoding, "Content-Encoding")                                   \
    XX(content_length, "Content-Length")                                       \
    XX(content_md5, "Content-MD5")                                             \
    XX(content_range, "Content-Range")                                         \
    XX(digest, "Digest")                                                       \
    XX(etag, "ETag")                                                           \
    XX(pragma, "Pragma")                                                       \
    XX(transfer_encoding, "Transfer-Encoding")                                 \
    XX(last_modified, "Last-Modified")

enum struct resp_hdr
{
#define XX(name, str) name,
    RESP_HDRS(XX)
#undef XX
        unknown,
};

struct intr_resp_hdrs
{
    // We want to inspect the value of such headers
    // clang-format off
    static constexpr const_string_t hdrs_[] =
    {
#define XX(name, str) str,
        RESP_HDRS(XX)
#undef XX
    };
    // We need to go to blind tunnel mode if such headers are present.
    static constexpr const_string_t unsupported_hdrs_[] =
    {
        "WWW-Authenticate",
    };
    // clang-format on
};

resp_hdr resp_hdr_idx(const string_view_t& hdr) noexcept;

inline constexpr const const_string_t& hdr_str(resp_hdr h) noexcept
{
    assert(h != resp_hdr::unknown);
    return intr_resp_hdrs::hdrs_[static_cast<std::underlying_type_t<resp_hdr>>(
        h)];
}

#undef RESP_HDRS

////////////////////////////////////////////////////////////////////////////////

template <typename Hdrs>
constexpr auto max_hdr_len() noexcept
{
    size_t max_len = 0;
    for (const auto& h : Hdrs::hdrs_)
    {
        if (max_len < h.size())
            max_len = h.size();
    }
    for (const auto& h : Hdrs::unsupported_hdrs_)
    {
        if (max_len < h.size())
            max_len = h.size();
    }
    return max_len;
}

inline bool is_same_hdr(const const_string_t& exp_hdr,
                        const string_view_t& hdr) noexcept
{
    return ((exp_hdr.size() == hdr.size()) &&
            (::strncasecmp(exp_hdr.data(), hdr.data(), hdr.size()) == 0));
}

template <typename ExpHdrIdx>
inline bool is_same_hdr(ExpHdrIdx exp_hdr, const string_view_t& hdr) noexcept
{
    // Don't want to slow down the compilation with this check
    // static_assert(std::is_same<ExpHdrIdx, req_hdr>::value ||
    //               std::is_same<ExpHdrIdx, resp_hdr>::value, "");
    return is_same_hdr(hdr_str(exp_hdr), hdr);
}

template <typename Hdrs>
bool hdr_unsupported(const string_view_t& hdr) noexcept
{
    for (const auto& h : Hdrs::unsupported_hdrs_)
    {
        if (is_same_hdr(h, hdr))
        {
            return true;
        }
    }
    return false;
}

} // namespace detail
} // namespace http
