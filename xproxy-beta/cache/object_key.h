#pragma once

#include "fs_node_key.h"
#include "range.h"

namespace cache
{
struct cache_key;

namespace detail
{

class object_key
{
    fs_node_key_t fs_node_key_;

    range rng_;

public:
#ifdef X3ME_TEST
    object_key() noexcept {}
    object_key(const fs_node_key_t& key, range rng) noexcept
        : fs_node_key_(key),
          rng_(rng)
    {
    }
#else
    object_key() = delete;
#endif

    object_key(const cache_key& ckey, bytes64_t skip_bytes) noexcept;

    const fs_node_key_t& fs_node_key() const noexcept { return fs_node_key_; }
    const range& get_range() const noexcept { return rng_; }
};

////////////////////////////////////////////////////////////////////////////////

class object_key_view
{
    const fs_node_key_t& fs_node_key_;
    const range& rng_;

public:
    object_key_view(const fs_node_key_t& key, const range& rng) noexcept
        : fs_node_key_(key),
          rng_(rng)
    {
    }

    const fs_node_key_t& fs_node_key() const noexcept { return fs_node_key_; }
    const range& get_range() const noexcept { return rng_; }
};

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const object_key& rhs) noexcept;
std::ostream& operator<<(std::ostream& os, const object_key_view& rhs) noexcept;

} // namespace detail
} // namespace cache
