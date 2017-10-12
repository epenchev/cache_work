#include "precompiled.h"
#include "cache_mgr.h"
#include "cache_common.h"
#include "cache_fs.h"
#include "cache_key.h"
#include "cache_stats.h"
#include "object_key.h"
#include "object_open_handle.h"
#include "settings.h"
#include "volume_info.h"

namespace cache
{
namespace
{

// TODO This naive function will cause the content from all disk to be lost
// if one of them fails.
uint32_t cache_key_to_fs_idx(const cache_key& ckey, uint32_t cnt_fss) noexcept
{
    // Change the hash function here, if the distribution across the volumes
    // is not good.
    // We want the ranges/stuff for the same url to go to the same disk,
    // so that we can collect and merge them later if needed.
    return boost::hash_range(ckey.url_.cbegin(), ckey.url_.cend()) % cnt_fss;
}

} // namespace
namespace detail
{
struct cache_fs_compare
{
    bool operator()(const cache_fs_ptr_t& lhs, const cache_fs_ptr_t& rhs) const
        noexcept
    {
        // We sort the filesystems by their uuid instead by their path, because
        // there are cases where same volume is assigned different letter
        // after unplug/plug, restart, etc.
        return lhs->uuid() < rhs->uuid();
    }
};
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

cache_mgr::cache_mgr() noexcept : ios_work_(ios_), sync_meta_tmr_(ios_)
{
}

cache_mgr::~cache_mgr() noexcept
{
}

bool cache_mgr::reset_volumes(const settings& sts) noexcept
{
    return init_reset(sts, true /*reset_volumes*/);
}

bool cache_mgr::start(const settings& sts) noexcept
{
    if (!init_reset(sts, false /*reset_volumes*/))
        return false;

    // It's safe to schedule the timer here directly, because it's corresponding
    // thread and io_service are started on the next line.
    schedule_metadata_sync();
    runner_ = std::thread([&]
                          {
                              using x3me::sys_utils::set_this_thread_name;
                              set_this_thread_name("xproxy_cmgr");
                              ios_.run();
                          });

    return true;
}

void cache_mgr::stop() noexcept
{
    ios_.stop();
    if (runner_.joinable())
        runner_.join();
    // Parallelize stopping of the cache file systems, because each one
    // may need to do blocking disk flush of its metadata.
    if (auto cfs = cache_fs_.read_copy())
    {
        std::vector<std::thread> thrs;
        thrs.reserve(cfs->size());
        for (auto& fs : *cfs)
        {
            thrs.emplace_back([fs]
                              {
                                  using x3me::sys_utils::set_this_thread_name;
                                  set_this_thread_name("xproxy_dsync");
                                  fs->close(false /*not forced*/);
                              });
        }
        for (auto& t : thrs)
        {
            if (t.joinable())
                t.join();
        }
    }
    // Destroy the file systems.
    cache_fs_.release();
}

std::vector<stats_fs> cache_mgr::get_stats() const noexcept
{
    std::vector<stats_fs> ret;

    auto cfs = cache_fs_.read_copy();
    // Can be null if this method is called when the cache_mgr is still
    // initializing or after the cache_mgr has been stopped.
    if (X3ME_LIKELY(cfs))
    {
        ret.reserve(cfs->size());
        std::transform(cfs->begin(), cfs->end(), std::back_inserter(ret),
                       [](const auto& fs)
                       {
                           return fs->get_stats();
                       });
    }

    return ret;
}

std::vector<stats_internal> cache_mgr::get_internal_stats() const noexcept
{
    std::vector<stats_internal> ret;

    auto cfs = cache_fs_.read_copy();
    // Can be null if this method is called when the cache_mgr is still
    // initializing or after the cache_mgr has been stopped.
    if (X3ME_LIKELY(cfs))
    {
        ret.reserve(cfs->size());
        std::transform(cfs->begin(), cfs->end(), std::back_inserter(ret),
                       [](const auto& fs)
                       {
                           return fs->get_internal_stats();
                       });
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

detail::object_ohandle_ptr_t
cache_mgr::async_open_read(const cache_key& ckey,
                           bytes64_t skip_bytes,
                           detail::open_rhandler_t&& h) noexcept
{
    auto cfs          = cache_fs_.read_copy();
    const auto fs_idx = cache_key_to_fs_idx(ckey, cfs->size());
    const auto& fs = *(cfs->begin() + fs_idx);
    const detail::object_key obj_key(ckey, skip_bytes);
    XLOG_INFO(disk_tag, "Issue async_open_read to cache FS '{}'. "
                        "Cache_key {}. Skip bytes {}. Obj_key {}",
              fs->vol_path(), ckey, skip_bytes, obj_key);
    return fs->async_open_read(obj_key, std::move(h));
}

detail::object_ohandle_ptr_t
cache_mgr::async_open_write(const cache_key& ckey,
                            bool truncate_object,
                            detail::open_whandler_t&& h) noexcept
{
    auto cfs             = cache_fs_.read_copy();
    const auto fs_idx    = cache_key_to_fs_idx(ckey, cfs->size());
    const auto& fs       = *(cfs->begin() + fs_idx);
    const bytes64_t skip = 0; // We don't skip bytes on write
    const detail::object_key obj_key(ckey, skip);
    XLOG_INFO(disk_tag, "Issue async_open_write to cache FS '{}'. Cache_key "
                        "{}. Truncate {}. Obj_key {}",
              fs->vol_path(), ckey, truncate_object, obj_key);
    return fs->async_open_write(obj_key, truncate_object, std::move(h));
}

////////////////////////////////////////////////////////////////////////////////

bool cache_mgr::init_reset(const settings& sts, bool reset_vols) noexcept
{
    const auto numt = sts.cache_volume_threads();
    if (numt < detail::aio_service::min_num_threads)
    {
        const auto t = detail::aio_service::min_num_threads;
        XLOG_FATAL(disk_tag, "Cache volume_threads must be at least {}", t);
        return false;
    }
    const bytes64_t obj_size = sts.cache_min_avg_object_size_KB() * 1024U;
    if (!x3me::math::in_range(obj_size,
                              static_cast<bytes64_t>(detail::min_obj_size),
                              static_cast<bytes64_t>(detail::max_obj_size) + 1))
    {
        XLOG_FATAL(disk_tag, "Invalid number for the setting cache "
                             "min_avg_object_szie. Must be in [{} - {}]",
                   detail::min_obj_size, detail::max_obj_size);
        return false;
    }
    if (x3me::sys_utils::memory_page_size() > detail::store_block_size)
    {
        XLOG_FATAL(disk_tag, "Can't work with page size bigger than {} bytes. "
                             "System page size is {} bytes. This is a "
                             "limitation which needs and can be fixed",
                   detail::store_block_size,
                   x3me::sys_utils::memory_page_size());
        return false;
    }

    // It's a bit wasteful to load all paths in the memory and then
    // process it. We may instead load and process it line by line.
    // However, this is done only at application start once i.e.
    // it's not performance critical. In addition the code is a bit
    // simpler in this way (IMO)
    const auto volume_paths = load_storage_cfg(sts.cache_storage_cfg());
    if (volume_paths.empty())
        return false;

    return init_volumes_fs(volume_paths, obj_size, numt, reset_vols);
}

cache_mgr::volume_paths_t
cache_mgr::load_storage_cfg(const std::string& fpath) noexcept
{
    volume_paths_t paths;

    std::ifstream ifs(fpath);
    if (ifs)
    {
        for (std::string s; std::getline(ifs, s);)
        {
            boost::algorithm::trim(s);
            if (!s.empty() && (s[0] != '#'))
                paths.emplace_back(s.data(), s.size());
        }

        if (paths.empty())
        {
            XLOG_FATAL(disk_tag, "No volume paths loaded from '{}'", fpath);
        }
    }
    else
    {
        const err_code_t err(errno, boost::system::get_system_category());
        XLOG_FATAL(disk_tag,
                   "Unable to load storage configuration from '{}'. {}", fpath,
                   err.message());
    }

    return paths;
}

bool cache_mgr::init_volumes_fs(const volume_paths_t& vpaths,
                                uint32_t min_avg_obj_size,
                                uint16_t num_aio_threads,
                                bool reset_vols) noexcept
{
    // Parallelize the initialization of the cache filesystems which do
    // metadata reads/writes upon start.
    std::vector<std::thread> thrs;
    thrs.reserve(vpaths.size());

    std::vector<detail::cache_fs_ptr_t> fss(vpaths.size());
    for (size_t i = 0; i < vpaths.size(); ++i)
    {
        const auto& vpath = vpaths[i];
        auto& fs          = fss[i];

        thrs.emplace_back(
            [this, &vpath, &fs, min_avg_obj_size, num_aio_threads, reset_vols]
            {
                x3me::sys_utils::set_this_thread_name("xproxy_dinit");
                try
                {
                    auto vi = detail::load_check_volume_info(vpath);
                    XLOG_INFO(disk_tag, "Loaded volume_info for '{}': {}",
                              vpath, vi);

                    auto on_fs_bad =
                        make_mem_fn_delegate(&cache_mgr::on_fs_bad, this);
                    auto new_fs = detail::cache_fs::create(vi, min_avg_obj_size,
                                                           on_fs_bad);
                    if (!reset_vols)
                    {
                        if (new_fs->init(num_aio_threads))
                            fs = std::move(new_fs);
                    }
                    else
                    {
                        if (new_fs->init_reset())
                            fs = std::move(new_fs);
                    }
                }
                catch (const std::exception& ex)
                {
                    XLOG_FATAL(disk_tag, "Unable to initialize volume "
                                         "in-memory structures for '{}'. {}",
                               vpath, ex.what());
                }
            });
    }

    for (auto& t : thrs)
    {
        if (t.joinable())
            t.join();
    }

    detail::cache_fs_ptr_t empty;
    if (std::find(fss.cbegin(), fss.cend(), empty) != fss.cend())
    {
        XLOG_FATAL(disk_tag, "Failed to {} one or more "
                             "volumes. Fix or remove the volumes from "
                             "the storage configuration file",
                   (reset_vols ? "reset" : "initialize"));
        return false;
    }

    if (!reset_vols)
        cache_fs_.update(x3me::thread::in_place, fss.begin(), fss.end());

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void cache_mgr::on_fs_bad(const detail::cache_fs_ptr_t& fs) noexcept
{
    // Post the event on the special io_service so that
    // 1. All potentially concurrent calls for bad FS are serialized. This one
    // is really important. Otherwise we'll need read/write lock around the
    // filesystems collection and it's usage.
    // 2. Free the given AIO thread for other useful work.
    // 3. Using the bad filesystems collection only from single thread
    // allows us to not lock it.
    ios_.post([this, fs]
              {
                  auto cfs = cache_fs_.read_copy();
                  if (cfs->find(fs) != cfs->cend())
                  {
                      // First update the collection with the current file
                      // systems
                      {
                          cache_fs_set_t tmp(*cfs);
                          tmp.erase(fs);

                          cache_fs_.update(x3me::thread::in_place,
                                           std::move(tmp));
                      }
                      XLOG_WARN(disk_tag, "Cache_mgr. Stopping bad FS '{}'",
                                fs->vol_path());
                      fs->close(true /*don't sync bad filesystem*/);
                  }
              });
}

////////////////////////////////////////////////////////////////////////////////

void cache_mgr::schedule_metadata_sync() noexcept
{
    constexpr auto timeout = std::chrono::minutes(20);
    err_code_t err;
    sync_meta_tmr_.expires_from_now(timeout, err);
    if (err)
    {
        // Shouldn't happen in practice.
        // Crash the application we can't work without this timer
        // and graceful shutdown may pass unnoticed.
        std::cerr << "FS_metadata_sync timer setup failure. " << err.message()
                  << std::endl;
        std::abort();
    }
    sync_meta_tmr_.async_wait(
        [this](const err_code_t& err)
        {
            if (!err)
            {
                start_metadata_sync(detail::cache_fs_ptr_t{});
            }
            else if (err != asio_error::operation_aborted)
            {
                // Shouldn't happen in practice. Crash the application since we
                // can't work without this timer and graceful shutdown may pass
                // unnoticed.
                std::cerr << "FS_metadata_sync timer failure. " << err.message()
                          << std::endl;
                std::abort();
            }
        });
}

void cache_mgr::start_metadata_sync(
    const detail::cache_fs_ptr_t& prev_fs) noexcept
{
    auto cfs = cache_fs_.read_copy();
    // We need to continue from the next cache FS.
    // Note that even if some file systems has been removed in the meantime
    // we'll continue from the next because our collection of cache filesystems
    // is sorted. If lower_bound returns end iterator, this means that we
    // have traversed all file systems we need to schedule another sync round.
    // Important note is that we never add back to the cache_fs container
    // during runtime. We do this only in the initialization phase.
    // This is also a must for the algorithm here to work.
    auto fs = [](const auto& fss, const auto& prev_fs)
    {
        auto it = prev_fs ? fss.lower_bound(prev_fs) : fss.begin();
        if (it != fss.end())
        {
            if (*it == prev_fs)
            {
                // The prev_fs is found in the collection.
                // We need to get the next, unless if we have reached the end.
                // We need to schedule another sync round in this case.
                it += 1;
                if (it != fss.end())
                    return *it;
            }
            else
                return *it;
        }
        return detail::cache_fs_ptr_t{};
    }(*cfs, prev_fs);
    if (!fs)
    { // Start from the beginning
        schedule_metadata_sync();
        return;
    }
    fs->async_sync_metadata([this](const detail::cache_fs_ptr_t& fs)
                            {
                                ios_.post([this, fs]
                                          {
                                              start_metadata_sync(fs);
                                          });
                            });
}

} // namespace cache
