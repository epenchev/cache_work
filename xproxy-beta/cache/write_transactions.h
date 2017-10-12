#pragma once

#include "fs_node_key.h"
#include "range.h"
#include "range_vector.h"

namespace cache
{
namespace detail
{
class write_transaction;

class write_transactions
{
    using data_t = boost::container::flat_map<fs_node_key_t, range_vector>;

    data_t data_;

public:
    // Returns a valid write_transaction if the entry has been added.
    // Returns invalid write_transaction otherwise.
    write_transaction add_entry(const fs_node_key_t& key, range rng) noexcept;
    void rem_entry(const write_transaction& wtrans) noexcept;
};

} // namespace detail
} // namespace cache
