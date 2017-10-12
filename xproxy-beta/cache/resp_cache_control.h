#pragma once

namespace cache
{
/*
    cc_not_present, // No Cache-Control in the response headers
    cc_no_cache, // no-cache, no-store and if 'Pragma: no-cache' is present
    cc_other, // revalidate; public/private, max-age=...; etc
*/
#define RESP_CACHE_CONTROL(XX)                                                 \
    XX(cc_not_present)                                                         \
    XX(cc_public)                                                              \
    XX(cc_no_cache)                                                            \
    XX(cc_private)                                                             \
    XX(cc_other)

enum struct resp_cache_control : uint8_t
{
#define XX(name) name,
    RESP_CACHE_CONTROL(XX)
#undef XX
};

std::ostream& operator<<(std::ostream& os,
                         const resp_cache_control& rhs) noexcept;
} // namespace cache
