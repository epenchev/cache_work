#pragma once

namespace cache
{
namespace detail
{

class agg_write_block;
struct cache_fs_ops;
class fs_metadata;

using agg_wblock_sync_t =
    x3me::thread::synchronized<agg_write_block, x3me::thread::shared_mutex>;
using cache_fs_ops_ptr_t = std::shared_ptr<cache_fs_ops>;
using fs_metadata_sync_t =
    x3me::thread::synchronized<fs_metadata, x3me::thread::shared_mutex>;

} // namespace detail
} // namespace cache
