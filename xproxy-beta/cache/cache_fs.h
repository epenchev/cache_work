#pragma once

#include "aio_service.h"
#include "async_handlers_fwds.h"
#include "cache_fs_operations.h"
#include "fs_metadata.h"
#include "unit_blocks.h"
#include "volume_fd.h"

namespace cache
{
struct stats_fs;
struct stats_internal;
namespace detail
{

class volume_info;
class object_key;

class agg_writer;
using agg_writer_ptr_t = aio_task_ptr_t<agg_writer>;

// The main reason why the cache_fs needs to be reference counted is
// that we need to handle the case when some disk returns fatal error
// and we need to remove the corresponding cache_fs from the managed
// filesystems. However, we need to do this action after there are no
// more references to the filesystem or it's data.
// We need the cache_fs to be standard shared_ptr instead of
// the lighter boost::intrusive_ptr because of the aliasing constructor
// provided by the std::shared_ptr which will give us ability to keep
// reference to the cache_fs when in fact we have a reference to its members.
// We need the thread safety of the reference counting too, but the boost
// one provides this too.
class cache_fs;
using cache_fs_ptr_t = std::shared_ptr<cache_fs>;

class cache_fs : public std::enable_shared_from_this<cache_fs>
{
    using on_fs_bad_cb_t =
        x3me::utils::mem_fn_delegate<void(const cache_fs_ptr_t&)>;

    volume_fd fd_;

    fs_metadata_sync_t fs_meta_;

    agg_writer_ptr_t agg_writer_;

    aio_service aios_; // The object is thread safe

    // The GCC 5.3 for Ubuntu 14.04 still doesn't have the SSO
    // in the std::string, but the boost one does.
    // 99% of the cases I expect our paths (block devices) to be short enough
    // so that the SSO will kick in.
    const boost::container::string path_;
    // This gets set to something meaningful after successful initalization
    uuid_t uuid_ = boost::uuids::nil_generator()();

    cache_fs_operations fs_ops_;

    // We skip some bytes from the beginning of the given volume in order
    // to not mess with the OS stuff.
    // We have two copies of the metadata on the disk.
    // They are placed one after another, thus we need to metadata_offsets.
    const std::array<bytes64_t, 2> metadata_offset_;

    // Theoretically we could receive simultaneous reports for disk
    // errors from the different AIO threads.
    // Thus we need some kind of locking here.
    // However, it's very unlikely the errors to happen so often to be
    // concurrent with one another (unless if we have very ugly bugs).
    // Thus we use a spin lock to be as light as possible.
    mutable x3me::thread::spin_lock err_mutex_;
    on_fs_bad_cb_t on_fs_bad_;
    uint16_t cnt_disk_errors_ = 0;

    std::atomic_bool async_sync_in_progress_{false};

    struct private_tag
    {
    };

public:
    cache_fs(const volume_info& vi,
             bytes32_t min_avg_obj_size,
             const on_fs_bad_cb_t& on_fs_bad,
             private_tag) noexcept;
    ~cache_fs() noexcept;

    cache_fs(const cache_fs&) = delete;
    cache_fs& operator=(const cache_fs&) = delete;
    cache_fs(cache_fs&& rhs) = delete;
    cache_fs& operator=(cache_fs&& rhs) = delete;

    static cache_fs_ptr_t create(const volume_info& vi,
                                 bytes32_t min_avg_obj_size,
                                 const on_fs_bad_cb_t& on_fs_bad) noexcept;
    // This function just initializes/resets the FS.
    // The FS is not functional after a call to this function.
    // It doesn't need corresponding close call.
    // Can be used only before a call to init.
    bool init_reset() noexcept;

    bool init(uint16_t num_threads) noexcept;

    // Stops an in-progress metdata sync (if any).
    // Syncs synchronously the metadata (if dirty).
    // Closes the FS.
    // Calling close and async_sync_metadata from different threads
    // simultaneously is not allowed.
    void close(bool forced) noexcept;

    object_ohandle_ptr_t async_open_read(const object_key& obj_key,
                                         open_rhandler_t&& h) noexcept;
    object_ohandle_ptr_t async_open_write(const object_key& obj_key,
                                          bool truncate_object,
                                          open_whandler_t&& h) noexcept;

    // These methods can be safely called from multiple threads
    stats_fs get_stats() const noexcept;
    stats_internal get_internal_stats() const noexcept;

    using cb_on_sync_end_t = std::function<void(const cache_fs_ptr_t&)>;
    // The on_end function won't be called if the cache_fs::close method
    // gets called while there is async sync of the metadata in progress.
    void async_sync_metadata(const cb_on_sync_end_t& on_end) noexcept;

    const boost::container::string& vol_path() const noexcept { return path_; }
    const uuid_t& uuid() const noexcept { return uuid_; }

private:
    void init_reset_impl(volume_fd& fd, fs_metadata& out);
    // Synchronous sync of the metadata
    void sync_metadata() noexcept;

    void on_disk_error() noexcept;

private:
    cache_fs_ops_ptr_t make_shared_fs_ops() noexcept;
    bytes64_t metadata_max_size() const noexcept;
};

} // namespace detail
} // namespace cache
