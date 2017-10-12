#pragma once

#include "async_handlers_fwds.h"

namespace cache
{
struct cache_key;

struct object_distributor
{
    virtual ~object_distributor() {}

    virtual detail::object_ohandle_ptr_t async_open_read(
        const cache_key&, bytes64_t, detail::open_rhandler_t&&) noexcept = 0;

    virtual detail::object_ohandle_ptr_t
    async_open_write(const cache_key&,
                     bool truncate_object,
                     detail::open_whandler_t&&) noexcept = 0;
};

} // namespace cache
