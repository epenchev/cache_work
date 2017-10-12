#include "precompiled.h"
#include "cache_fs.h"
#include "agg_writer.h"
#include "aligned_data_ptr.h"
#include "cache_stats.h"
#include "disk_reader.h"
#include "memory_writer.h"
#include "object_open_handle.h"
#include "task_md_sync.h"
#include "volume_info.h"

namespace cache
{
namespace detail
{
// Every call to the metadata in these functions invokes an operation on
// synchronized object which is far from effective, because it's not needed
// in the current context.
// However, these operations are used only upon construction, so it's not
// so big problem (IMO).
static auto data_offset(const volume_info& vi,
                        const fs_metadata_sync_t& md) noexcept
{
    const auto md_size = md->max_size_on_disk();
    return store_blocks64_t::create_from_bytes(vi.skip_bytes() + (2 * md_size));
}

static auto cnt_data_blocks(const volume_info& vi,
                            const fs_metadata_sync_t& md) noexcept
{
    const auto offs = data_offset(vi, md).to_bytes();
    return store_blocks64_t::create_from_bytes(vi.size() - offs);
}

static auto calc_metadata_offset(const volume_info& vi,
                                 const fs_metadata_sync_t& md) noexcept
{
    return std::array<bytes64_t, 2>{
        {vi.skip_bytes(), vi.skip_bytes() + md->max_size_on_disk()}};
}

////////////////////////////////////////////////////////////////////////////////

cache_fs::cache_fs(const volume_info& vi,
                   bytes32_t min_avg_obj_size,
                   const on_fs_bad_cb_t& on_fs_bad,
                   private_tag) noexcept
    : fs_meta_(x3me::thread::in_place, vi, min_avg_obj_size),
      aios_(fd_),
      path_(vi.path()),
      fs_ops_(&fd_,
              &fs_meta_,
              &aios_,
              &path_,
              data_offset(vi, fs_meta_),
              cnt_data_blocks(vi, fs_meta_)),
      metadata_offset_(calc_metadata_offset(vi, fs_meta_)),
      on_fs_bad_(on_fs_bad)
{
    X3ME_ENFORCE((metadata_offset_[0] % store_block_size) == 0);
    X3ME_ENFORCE((metadata_offset_[1] % store_block_size) == 0);
    fs_ops_.set_on_disk_error_cb(
        make_mem_fn_delegate(&cache_fs::on_disk_error, this));
    XLOG_DEBUG(disk_tag, "Create Cache_FS for volume '{}'. "
                         "Data offset: {} bytes. End data offset: {} bytes",
               path_, fs_ops_.data_offs(), fs_ops_.end_data_offs());
}

cache_fs::~cache_fs() noexcept
{
    // The destructor must not do anything dangerous/fancy like using
    // global objects or something like that, because the FS can be destroyed
    // by any thread.
    XLOG_INFO(disk_tag, "Destroy Cache_FS for volume '{}'", path_);
}

cache_fs_ptr_t cache_fs::create(const volume_info& vi,
                                bytes32_t min_avg_obj_size,
                                const on_fs_bad_cb_t& on_fs_bad) noexcept
{
    return std::make_shared<cache_fs>(vi, min_avg_obj_size, on_fs_bad,
                                      private_tag{});
}

bool cache_fs::init_reset() noexcept
{
    try
    {
        volume_fd fd;
        err_code_t err;
        if (!fd.open(path_.c_str(), err))
        {
            throw bsys::system_error(err, "Volume open failed");
        }

        // The copy here is cheap because the fs_meta_ member is still empty.
        auto tmp = fs_meta_.copy();
        init_reset_impl(fd, tmp);

        XLOG_INFO(disk_tag, "Reseted cache FS for volume '{}'. MD A "
                            "offset: {} bytes. MD B offset: {} bytes. Data "
                            "offset: {} bytes. End data offset: {} bytes. "
                            "Metadata:\n{}",
                  path_, metadata_offset_[0], metadata_offset_[1],
                  fs_ops_.data_offs(), fs_ops_.end_data_offs(), tmp);
    }
    catch (const std::exception& ex)
    {
        XLOG_FATAL(disk_tag,
                   "Unable to reset the cache FS for volume '{}'. {}. "
                   "MD A offset: {} bytes. MD B offset: {} bytes. Data "
                   "offset: {} bytes. End data offset: {} bytes",
                   path_, ex.what(), metadata_offset_[0], metadata_offset_[1],
                   fs_ops_.data_offs(), fs_ops_.end_data_offs());
        return false;
    }
    return true;
}

bool cache_fs::init(uint16_t num_threads) noexcept
{
    XLOG_DEBUG(disk_tag, "Start initialization of the cache FS for volume '{}'",
               path_);

    try
    {
        volume_fd fd;
        // The copy here is cheap because the fs_meta_ member is still empty.
        auto tmp = fs_meta_.copy();

        err_code_t err;
        if (!fd.open(path_.c_str(), err))
        {
            throw bsys::system_error(err, "Volume open failed");
        }
        auto check_write_pos = [](const auto& fs_ops, const fs_metadata& m)
        {
            if ((m.write_pos() < fs_ops.data_offs()) ||
                (m.write_pos() >= fs_ops.end_data_offs()))
            {
                // It's an error to load correctly the metadata and then to
                // found that the write_position is incorrect.
                XLOG_ERROR(
                    disk_tag,
                    "Write position {} is out of the valid range [{} - {})",
                    m.write_pos(), fs_ops.data_offs(), fs_ops.end_data_offs());
                return false;
            }
            return true;
        };
        // The metadata load/save functionality is a bit uneven.
        // It knows which copy of the metadata to load, but it doesn't know
        // which copy of the metadata to save. The problem is that we have
        // different kinds of metadata saving and I couldn't generalize them,
        // so that I can put them in the metadata functionality.
        disk_reader rdr(path_, metadata_offset_[0], fs_ops_.data_offs());
        if (!tmp.load(rdr) || !check_write_pos(fs_ops_, tmp))
        {
            init_reset_impl(fd, tmp);
        }

        if ((tmp.write_pos() + agg_write_block_size) > fs_ops_.end_data_offs())
            tmp.wrap_write_pos(fs_ops_.data_offs());

        XLOG_INFO(disk_tag, "Initialized cache FS for volume '{}'. MD A "
                            "offset: {} bytes. MD B offset: {} bytes. Data "
                            "offset: {} bytes. End data offset: {} bytes. "
                            "Metadata:\n{}",
                  path_, metadata_offset_[0], metadata_offset_[1],
                  fs_ops_.data_offs(), fs_ops_.end_data_offs(), tmp);
        const auto wpos = volume_blocks64_t::create_from_bytes(tmp.write_pos());
        // Now it's safe to populate the member variables
        fd_         = std::move(fd);
        uuid_       = tmp.uuid();
        agg_writer_ = make_aio_task<agg_writer>(wpos, tmp.write_lap());
        fs_meta_    = std::move(tmp);

        fs_ops_.set_agg_writer(agg_writer_.get());
        aios_.start(path_, num_threads);
        agg_writer_->start(&fs_ops_);
    }
    catch (const std::exception& ex)
    {
        XLOG_FATAL(disk_tag,
                   "Unable to initialize the cache FS for volume '{}'. {}. "
                   "MD A offset: {} bytes. MD B offset: {} bytes. Data "
                   "offset: {} bytes. End data offset: {} bytes",
                   path_, ex.what(), metadata_offset_[0], metadata_offset_[1],
                   fs_ops_.data_offs(), fs_ops_.end_data_offs());
        return false;
    }
    return true;
}

void cache_fs::close(bool forced) noexcept
{
    XLOG_DEBUG(disk_tag, "Closing the cache FS for volume '{}'. Forced {}",
               path_, forced);

    // Once the aio_service is stopped there can't be any asynchronous disk
    // operations in progress.
    aios_.stop();

    if (!forced)
    {
        // We need to flush the aggregate writer before the metadata, because
        // the flush of the last may change some of the metadata fields.
        agg_writer_->stop_flush();

        if (async_sync_in_progress_.exchange(false))
        {
            // The aio_service is stopped. The async metadata save no longer
            // runs.
            XLOG_DEBUG(disk_tag,
                       "Aborted metadata sync in progress for volume '{}'",
                       path_);
            fs_meta_->dec_sync_serial();

            // We stopped the asynchronous metadata sync and now we need to
            // save synchronously the metadata.
            sync_metadata();
        }
        else if (fs_meta_->is_dirty())
        {
            sync_metadata();
        }
    }

    // This ensures that the aggregate writer won't keep the cache_fs alive.
    agg_writer_.reset();

    err_code_t err;
    if (!fd_.close(err))
    {
        XLOG_ERROR(disk_tag, "Error when closing the file descriptor for "
                             "volume '{}'. Forced {}. {}",
                   path_, forced, err.message());
    }
}

////////////////////////////////////////////////////////////////////////////////

object_ohandle_ptr_t cache_fs::async_open_read(const object_key& obj_key,
                                               open_rhandler_t&& h) noexcept
{
    object_ohandle_ptr_t ret;
    // TODO: Temporary, quick and dirty solution to prevent slowing down
    // the user experience due to too many disk read operations.
    // 7 reader threads per volume i.e. average 8 pending tasks per thread.
    if (aios_.read_queue_size() < 56)
    {
        ret = make_aio_task<object_open_read_handle>(make_shared_fs_ops(),
                                                     obj_key, std::move(h));
        // Post in the front of the queue so that the open operation returns
        // a result as soon as possible.
        aios_.push_front_read_queue(ret.get());
    }
    return ret;
}

object_ohandle_ptr_t cache_fs::async_open_write(const object_key& obj_key,
                                                bool truncate_object,
                                                open_whandler_t&& h) noexcept
{
    object_ohandle_ptr_t ret;
    // TODO: Temporary, quick and dirty solution to prevent slowing down
    // the user experience due to too many disk write operations.
    // 1 writer thread per volume i.e. at max 56 pending tasks per thread.
    if (aios_.write_queue_size() < 56)
    {
        ret = make_aio_task<object_open_write_handle>(
            make_shared_fs_ops(), obj_key, truncate_object, std::move(h));
        // Post in the front of the queue so that the open operation returns
        // a result as soon as possible.
        aios_.push_front_read_queue(ret.get());
    }
    return ret;
}

stats_fs cache_fs::get_stats() const noexcept
{
    stats_fs sts;

    auto& smd = static_cast<stats_fs_md&>(sts);
    auto& swr = static_cast<stats_fs_wr&>(sts);
    auto& sop = static_cast<stats_fs_ops&>(sts);

    fs_ops_.get_stats(smd, sop);
    agg_writer_->get_stats(swr);

    sts.path_               = path_;
    sts.cnt_pending_reads_  = aios_.read_queue_size();
    sts.cnt_pending_writes_ = aios_.write_queue_size();

    err_mutex_.lock();
    sts.cnt_errors_ = cnt_disk_errors_;
    err_mutex_.unlock();

    return sts;
}

stats_internal cache_fs::get_internal_stats() const noexcept
{
    stats_internal sts;
    fs_ops_.get_internal_stats(sts);
    return sts;
}

////////////////////////////////////////////////////////////////////////////////

void cache_fs::init_reset_impl(volume_fd& fd, fs_metadata& out)
{

    XLOG_WARN(disk_tag, "Creating new cache FS for volume '{}'", path_);

    out.clean_init(fs_ops_.data_offs());

    const auto buff_size = metadata_max_size();
    auto p               = alloc_page_aligned(buff_size);

    // Write the A copy of the metadata
    memory_writer wtr(p.get(), buff_size);
    out.save(wtr);
    const auto md_size = wtr.written();
    err_code_t err;
    if (!fd.write(p.get(), md_size, metadata_offset_[0], err))
    {
        throw bsys::system_error(err, "Write A metadata failed");
    }
    // Now write the B copy of the metadata so that we can safely
    // decide which metadata copy to use, if we get restarted before
    // any of them is written again.
    if (!fd.write(p.get(), md_size, metadata_offset_[1], err))
    {
        throw bsys::system_error(err, "Write B metadata failed");
    }
}

void cache_fs::async_sync_metadata(const cb_on_sync_end_t& on_end) noexcept
{
    if (fs_meta_->is_dirty())
    {
        const auto prev = async_sync_in_progress_.exchange(true);
        X3ME_ASSERT(!prev, "There shouldn't be a previous metadata sync "
                           "operation in progress");

        using x3me::thread::with_synchronized;
        const uint32_t idx = with_synchronized(fs_meta_, [](fs_metadata& md)
                                               {
                                                   md.set_non_dirty();
                                                   md.inc_sync_serial();
                                                   return md.sync_serial() & 1U;
                                               });
        const auto offs = metadata_offset_[idx];
        const auto cp   = idx ? 'B' : 'A';

        const auto buff_size = metadata_max_size();
        auto p               = alloc_page_aligned(buff_size);

        memory_writer wtr(p.get(), buff_size);
        fs_meta_.as_const()->save(wtr);

        const auto md_size = wtr.written();
        XLOG_INFO(disk_tag, "Start {} metadata asynchronous sync for cache "
                            "FS for volume '{}'. Disk offset {} bytes. Size "
                            "{} bytes",
                  cp, path_, offs, md_size);

        auto cb = [ inst = shared_from_this(), on_end ](bool res)
        {
            inst->async_sync_in_progress_ = false;
            if (!res)
            {
                inst->fs_meta_->dec_sync_serial();
                inst->on_disk_error();
            }
            on_end(inst);
        };
        auto t = make_aio_task<task_md_sync>(&fs_ops_, std::move(p), md_size,
                                             offs, cb);
        // The queue internally keeps the task alive using it's reference count.
        aios_.push_write_queue(t.get());
    }
    else
    {
        XLOG_INFO(disk_tag, "Skip asynchronous sync of non-dirty metadata "
                            "for cache FS for volume '{}'",
                  path_);
        on_end(shared_from_this());
    }
}

void cache_fs::sync_metadata() noexcept
{
    // Note that when this function is called all IO threads are already
    // stopped i.e. although the code here is not logically thread safe
    // (physically it is) there shouldn't be any problems.
    using x3me::thread::with_synchronized;
    const uint32_t idx = with_synchronized(fs_meta_, [](fs_metadata& md)
                                           {
                                               md.set_non_dirty();
                                               md.inc_sync_serial();
                                               return md.sync_serial() & 1U;
                                           });
    const auto offs = metadata_offset_[idx];
    const auto cp   = idx ? 'B' : 'A';

    const auto buff_size = metadata_max_size();
    auto p               = alloc_page_aligned(buff_size);

    memory_writer wtr(p.get(), buff_size);
    fs_meta_.as_const()->save(wtr);

    const auto md_size = wtr.written();
    XLOG_DEBUG(
        disk_tag,
        "Start {} metadata synchronous sync for cache FS for volume '{}'", cp,
        path_);

    err_code_t err;
    if (fd_.write(p.get(), md_size, offs, err))
    {
        XLOG_INFO(disk_tag,
                  "Updated {} metadata for cache FS for volume "
                  "'{}'. Disk offset: {} bytes. Size: {} bytes. Metadata:\n{}",
                  cp, path_, offs, md_size, fs_meta_.as_const());
    }
    else
    {
        fs_meta_->dec_sync_serial();
        XLOG_ERROR(
            disk_tag,
            "Failed to update {} metadata for cache FS for volume "
            "'{}'. {}. Disk offset: {} bytes. Size: {} bytes. Metadata:\n{}",
            cp, path_, err.message(), offs, md_size, fs_meta_.as_const());
    }
}

////////////////////////////////////////////////////////////////////////////////

void cache_fs::on_disk_error() noexcept
{
    // We want to do the heavy actions, such as logging and callback,
    // outside the critical section.
    auto cb = [this]
    {
        on_fs_bad_cb_t cb;

        // If we get so many fatal errors for a given cache FS (volume)
        // we are going to declare the FS bad and remove it (not use it).
        // Currently only OS read/write errors are counted.
        constexpr uint16_t max_cnt_disk_errors = 5;

        std::lock_guard<decltype(err_mutex_)> _(err_mutex_);
        const auto cnt_errs = cnt_disk_errors_ + 1;
        // We want to stop counting the errors when they pass the limit
        if (cnt_errs <= max_cnt_disk_errors)
        {
            cnt_disk_errors_ = cnt_errs;
            if (cnt_errs == max_cnt_disk_errors)
            {
                using std::swap;
                std::swap(cb, on_fs_bad_);
            }
        }

        return cb;
    }(); // Note the call
    if (cb)
    {
        XLOG_ERROR(
            disk_tag,
            "Max count errors reached for volume '{}'. Informing cache manager",
            path_);
        cb(shared_from_this());
    }
}

////////////////////////////////////////////////////////////////////////////////

cache_fs_ops_ptr_t cache_fs::make_shared_fs_ops() noexcept
{
    return cache_fs_ops_ptr_t(shared_from_this(),
                              static_cast<cache_fs_ops*>(&fs_ops_));
}

bytes64_t cache_fs::metadata_max_size() const noexcept
{
    return metadata_offset_[1] - metadata_offset_[0];
}

} // namespace detail
} // namespace cache
