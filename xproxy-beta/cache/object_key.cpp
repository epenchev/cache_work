#include "precompiled.h"
#include "object_key.h"
#include "cache_key.h"
#include "xutils/http_funcs.h"

namespace cache
{
namespace detail
{

static fs_node_key_t calc_node_key(const cache_key& ckey) noexcept
{
    X3ME_ENFORCE(!ckey.url_.empty() && (ckey.obj_full_len_ > 0),
                 "Both fields must be present and valid");
    xutils::md5_hasher h;
    if (!ckey.etag_.empty())
    {
        // If the etag is present truncate the url to host with 2 levels
        // e.g. http://smth.google.com/more/path => google.com
        const auto host = xutils::truncate_host(xutils::get_host(ckey.url_), 2);
        X3ME_ASSERT(!host.empty(), "Got url without host");
        h.update(host.data(), host.size());
        h.update(ckey.etag_.data(), ckey.etag_.size());
    }
    else
    {
        // The etag is not present use the cache_url, if available
        const auto& url = ckey.cache_url_.empty() ? ckey.url_ : ckey.cache_url_;
        h.update(url.data(), url.size());
    }
    h.update(&ckey.obj_full_len_, sizeof(ckey.obj_full_len_));
    h.update(&ckey.last_modified_, sizeof(ckey.last_modified_));
    return h.final_hash();
}

static range calc_range(const cache_key& ckey, bytes64_t skip) noexcept
{
    range ret;
    if (ckey.rng_.valid())
    {
        const auto len = ckey.rng_.len();
        X3ME_ENFORCE(skip < len, "We must not skip the whole range");
        ret = range{ckey.rng_.beg_ + skip, len - skip};
    }
    else
    {
        X3ME_ENFORCE(skip < ckey.obj_full_len_,
                     "We must not skip the whole content length");
        ret = range{skip, ckey.obj_full_len_ - skip};
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////

object_key::object_key(const cache_key& ckey, bytes64_t skip_bytes) noexcept
    : fs_node_key_(calc_node_key(ckey)),
      rng_(calc_range(ckey, skip_bytes))

{
}

std::ostream& operator<<(std::ostream& os, const object_key& rhs) noexcept
{
    // clang-format off
    return os 
        << "{fs_node_key: " << rhs.fs_node_key()
        << ", rng: " << rhs.get_range() << '}';
    // clang-format on
}

std::ostream& operator<<(std::ostream& os, const object_key_view& rhs) noexcept
{
    // clang-format off
    return os 
        << "{fs_node_key: " << rhs.fs_node_key()
        << ", rng: " << rhs.get_range() << '}';
    // clang-format on
}
} // namespace detail
} // namespace cache
