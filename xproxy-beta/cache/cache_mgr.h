#pragma once

#include "object_distributor.h"

class settings;

namespace cache
{
struct cache_key;
struct stats_fs;
struct stats_internal;
namespace detail
{
class cache_fs;
class cache_fs_compare;
using cache_fs_ptr_t = std::shared_ptr<cache_fs>;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

class cache_mgr final : public object_distributor
{
    using cache_fs_set_t = boost::container::flat_set<detail::cache_fs_ptr_t,
                                                      detail::cache_fs_compare>;

    // This collection is going to be read-only accessed from the network
    // threads when a cache FS is chosen for read or write operation, based
    // on the object key. However, we need to remove from this collection
    // if we detect a bad FS/disk. The latter should be pretty rare situation.
    // So we need the collection to be shared lockable. Using synchronized
    // with shared_mutex will work. However, there is more efficient way
    // to do this (according to my measurements).
    // Thus we use our rcu_resource functionality.
    // In addition we need the alive FS to be sorted because this allows
    // use to continue from the next FS when doing async metadata sync,
    // if one or few FS get removed in the meantime.
    // TODO Use the more efficient atomic_shared_ptr when available
    // or from Antony Williams.
    x3me::thread::rcu_resource<cache_fs_set_t> cache_fs_;

    // It's kind of unfortunate that we need to start a separate thread here
    // for a single timer. Maybe in the future, we'll be able to provide
    // one common worker io_service and thread for all non networking and
    // non disk tasks (such as the management functionality and this one).
    io_service_t ios_;
    io_service_t::work ios_work_;
    std::thread runner_;
    std_timer_t sync_meta_tmr_;

public:
    cache_mgr() noexcept;
    ~cache_mgr() noexcept;

    cache_mgr(const cache_mgr&) = delete;
    cache_mgr& operator=(const cache_mgr&) = delete;
    cache_mgr(cache_mgr&&) = delete;
    cache_mgr& operator=(cache_mgr&&) = delete;

    /// Can be used only if the cache_mgr is not started
    bool reset_volumes(const settings& sts) noexcept;

    /// Initializes and starts the cache functionality
    /// Returns true if the initialization finished successfully.
    /// Returns false otherwise. The error is logged.
    bool start(const settings& sts) noexcept;

    /// Stops the cache functionality. The call is blocking.
    /// When it returns the cache functionality is stopped and
    /// no other cache requests will be processed.
    void stop() noexcept;

    // These methods can safely be called from multiple threads
    std::vector<stats_fs> get_stats() const noexcept;
    std::vector<stats_internal> get_internal_stats() const noexcept;

private:
    detail::object_ohandle_ptr_t
    async_open_read(const cache_key& ckey,
                    bytes64_t skip_bytes,
                    detail::open_rhandler_t&& h) noexcept final;

    detail::object_ohandle_ptr_t
    async_open_write(const cache_key& ckey,
                     bool truncate_object,
                     detail::open_whandler_t&& h) noexcept final;

private:
    bool init_reset(const settings& sts, bool reset_vols) noexcept;

    using volume_paths_t = std::vector<boost::container::string>;
    static volume_paths_t load_storage_cfg(const std::string& fpath) noexcept;

    bool init_volumes_fs(const volume_paths_t& vpaths,
                         uint32_t min_avg_obj_size,
                         uint16_t num_aio_threads,
                         bool reset_vols) noexcept;

    void on_fs_bad(const detail::cache_fs_ptr_t& fs) noexcept;

    void schedule_metadata_sync() noexcept;
    void
    start_metadata_sync(const detail::cache_fs_ptr_t& prev_synced) noexcept;
};

} // namespace cache
