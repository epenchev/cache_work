#pragma once

#include "hdr_value_pos.h"
#include "hdr_values_store.h"
#include "interesting_hdrs.h"
#include "cache/resp_cache_control.h"

namespace http
{
// These messages currently has only the information in which we are interested

struct req_msg
{
    static constexpr auto no_len = std::numeric_limits<bytes64_t>::max();
    static constexpr auto max_hdr_len =
        detail::max_hdr_len<detail::intr_req_hdrs>();

    using values_store_t = detail::hdr_values_store<max_hdr_len>;

    // We'll go to HTTP tunnel mode if we have URL bigger than this size.
    // We can't store arbitrary size URL just to log them.
    static constexpr bytes32_t max_url_len = 1_KB;
    boost_string_t url_;
    // This one is going to be set for some transactions but not for others.
    // It is used for the cache key, if set.
    boost_string_t cache_url_;

    bytes64_t content_len_ = no_len;

    values_store_t values_;
};

struct resp_msg
{
    static constexpr auto no_len = std::numeric_limits<bytes64_t>::max();
    static constexpr auto max_hdr_len =
        detail::max_hdr_len<detail::intr_resp_hdrs>();

    using values_store_t = detail::hdr_values_store<max_hdr_len>;

    struct rng // inclusive range [beg, end]
    {
        bytes64_t beg_ = static_cast<bytes64_t>(-1);
        bytes64_t end_ = 0;

        bool valid() const noexcept { return (end_ >= beg_); }
        bytes64_t len() const noexcept
        { // The range is inclusive, thus +1.
            return valid() ? ((end_ - beg_) + 1) : 0;
        }
        friend std::ostream& operator<<(std::ostream& os,
                                        const rng& rhs) noexcept
        {
            if (!rhs.valid())
                return os << "[0-0]";
            return os << '[' << rhs.beg_ << '-' << rhs.end_ << ']';
        }
    };

    rng rng_;
    bytes64_t content_len_ = no_len;
    bytes64_t object_len_  = no_len;
    time_t last_modified_  = 0;
    detail::hdr_value_pos content_encoding_;
    detail::hdr_value_pos content_md5_;
    detail::hdr_value_pos digest_sha1_;
    detail::hdr_value_pos digest_md5_;
    detail::hdr_value_pos etag_;

    values_store_t values_;

    cache::resp_cache_control cache_control_ =
        cache::resp_cache_control::cc_not_present;
};

} // namespace http
