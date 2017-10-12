#include "precompiled.h"
#include "cache_key.h"
#include "cache_common.h"
#include "range.h"

namespace cache
{

bool rw_op_allowed(const cache_key& key, bytes64_t skip /*= 0*/) noexcept
{
    if ((key.last_modified_ == 0) &&
        (key.resp_cache_control_ != resp_cache_control::cc_not_present) &&
        (key.resp_cache_control_ != resp_cache_control::cc_public))
    {
        // Disable cache operations if the Last-Modified header is not present,
        // and there is some non-public Cache-Control or Pragma: no-cache
        return false;
    }
    if (key.rng_.valid())
    {
        const auto len = key.rng_.len();
        X3ME_ENFORCE(skip <= len, "Invalid skip bytes");
        return detail::range::is_valid(key.rng_.beg_ + skip, len - skip);
    }
    X3ME_ENFORCE(skip <= key.obj_full_len_, "Invalid skip bytes");
    return detail::range::is_valid(skip, key.obj_full_len_ - skip);
}

std::ostream& operator<<(std::ostream& os, const cache_key& rhs) noexcept
{
    os << "{url: " << rhs.url_ << ", obj_len: " << rhs.obj_full_len_;
    if (rhs.rng_.valid())
        os << ", rng: " << '[' << rhs.rng_.beg_ << '-' << rhs.rng_.end_ << ']';
    os << ", last_mod: " << rhs.last_modified_;
    if (!rhs.content_encoding_.empty())
        os << ", cont_enc: " << rhs.content_encoding_;
    if (!rhs.content_md5_.empty())
        os << ", cont_md5: " << rhs.content_md5_;
    if (!rhs.digest_sha1_.empty())
        os << ", dig_sha1: " << rhs.digest_sha1_;
    if (!rhs.digest_md5_.empty())
        os << ", dig_md5: " << rhs.digest_md5_;
    if (!rhs.etag_.empty())
        os << ", etag: " << rhs.etag_;
    if (rhs.resp_cache_control_ != resp_cache_control::cc_not_present)
        os << ", resp_ccontr: " << rhs.resp_cache_control_;
    return os << '}';
}

} // namespace cache
