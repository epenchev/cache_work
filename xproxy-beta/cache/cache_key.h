#pragma once

#include "resp_cache_control.h"

namespace cache
{

struct cache_key
{

    struct rng // inclusive range [beg, end]
    {
        bytes64_t beg_ = static_cast<bytes64_t>(-1);
        bytes64_t end_ = 0;

        bytes64_t len() const noexcept { return (end_ - beg_) + 1; }
        bool valid() const noexcept { return (end_ >= beg_); }
    };

    // These fields are allowed to be empty
    string_view_t content_encoding_;
    string_view_t content_md5_;
    string_view_t digest_sha1_;
    string_view_t digest_md5_;
    string_view_t etag_;

    string_view_t url_; // This field must be present
    string_view_t cache_url_; // This field may or may not be present.

    uint64_t obj_full_len_ = 0; // bytes - must be present

    time_t last_modified_ = 0; // Could be something or remain zero

    rng rng_; // May not be present i.e. may be invalid

    resp_cache_control resp_cache_control_ = resp_cache_control::cc_not_present;
};

// The skip_len is used for the read operations only
bool rw_op_allowed(const cache_key& key, bytes64_t skip_len = 0) noexcept;

std::ostream& operator<<(std::ostream& os, const cache_key& rhs) noexcept;

} // namespace cache
