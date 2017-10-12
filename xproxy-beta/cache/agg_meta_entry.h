#pragma once

#include "fs_node_key.h"
#include "range_elem.h"

namespace cache
{
namespace detail
{

struct agg_meta_entry
{
    range_elem rng_;
    fs_node_key_t key_;

public:
    agg_meta_entry() noexcept = default;
    agg_meta_entry(const fs_node_key_t& k, const range_elem& r) noexcept
        : rng_(r),
          key_(k)
    {
    }
    const fs_node_key_t& key() const noexcept { return key_; }
    const range_elem& rng() const noexcept { return rng_; }

    friend std::ostream& operator<<(std::ostream& os,
                                    const agg_meta_entry& rhs) noexcept
    {
        return os << "{Key: " << rhs.key_ << ", Rng: " << rhs.rng_ << '}';
    }
};

////////////////////////////////////////////////////////////////////////////////

struct agg_meta_entry_view
{
    const range_elem& rng_;
    const fs_node_key_t& key_;

public:
    agg_meta_entry_view() noexcept = default;
    agg_meta_entry_view(const fs_node_key_t& k, const range_elem& r) noexcept
        : rng_(r),
          key_(k)
    {
    }
    const fs_node_key_t& key() const noexcept { return key_; }
    const range_elem& rng() const noexcept { return rng_; }

    friend std::ostream& operator<<(std::ostream& os,
                                    const agg_meta_entry_view& rhs) noexcept
    {
        return os << "{Key: " << rhs.key_ << ", Rng: " << rhs.rng_ << '}';
    }
};

////////////////////////////////////////////////////////////////////////////////
// Lexicographical compares like in "std::pair".
inline bool operator<(const agg_meta_entry& lhs,
                      const agg_meta_entry& rhs) noexcept
{
    return (lhs.key() < rhs.key()) ||
           (!(rhs.key() < lhs.key()) && (lhs.rng() < rhs.rng()));
}

inline bool operator<(const agg_meta_entry& lhs,
                      const agg_meta_entry_view& rhs) noexcept
{
    return (lhs.key() < rhs.key()) ||
           (!(rhs.key() < lhs.key()) && (lhs.rng() < rhs.rng()));
}

inline bool operator<(const agg_meta_entry_view& lhs,
                      const agg_meta_entry& rhs) noexcept
{
    return (lhs.key() < rhs.key()) ||
           (!(rhs.key() < lhs.key()) && (lhs.rng() < rhs.rng()));
}

inline bool operator==(const agg_meta_entry& lhs,
                       const agg_meta_entry& rhs) noexcept
{
    return (lhs.key() == rhs.key()) && (lhs.rng() == rhs.rng());
}

inline bool operator==(const agg_meta_entry& lhs,
                       const agg_meta_entry_view& rhs) noexcept
{
    return (lhs.key() == rhs.key()) && (lhs.rng() == rhs.rng());
}

inline bool operator==(const agg_meta_entry_view& lhs,
                       const agg_meta_entry& rhs) noexcept
{
    return (lhs.key() == rhs.key()) && (lhs.rng() == rhs.rng());
}

} // namespace detail
} // namespace cache
