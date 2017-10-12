#include "precompiled.h"
#include "task_md_sync.h"
#include "cache_common.h"
#include "cache_fs_ops.h"

namespace cache
{
namespace detail
{

task_md_sync::task_md_sync(non_owner_ptr_t<cache_fs_ops> fso,
                           aligned_data_ptr_t&& raw_data,
                           bytes64_t data_size,
                           bytes64_t disk_offset,
                           const on_end_cb_t& cb_on_end) noexcept
    : fs_ops_(fso),
      cb_on_end_(cb_on_end),
      raw_data_(std::move(raw_data)),
      written_size_(0),
      data_size_(data_size),
      disk_offset_(disk_offset),
      vol_path_(fso->vol_path())
{
    setup_write();
}

void task_md_sync::exec() noexcept
{
    X3ME_ASSERT(false, "This task do only IO write operations");
}

void task_md_sync::on_end_io_op(const err_code_t& err) noexcept
{
    if (!err)
    {
        written_size_ += aio_data_.size_;
        if (written_size_ < data_size_)
        {
            setup_write();
            fs_ops_->aios_push_write_queue(this);
        }
        else
        {
            X3ME_ASSERT(written_size_ == data_size_,
                        "Wrong logic for setup_write");
            XLOG_INFO(disk_tag, "Done asynchronous metadata sync for cache "
                                "FS for volume '{}'. Start disk offset {}. All "
                                "bytes {}. Written bytes {}",
                      vol_path_, disk_offset_, data_size_, written_size_);
            cb_on_end_(true);
        }
    }
    else
    {
        XLOG_FATAL(disk_tag,
                   "Error during asynchronous metadata sync for cache "
                   "FS for volume '{}'. {}, Start disk offset {}. All bytes "
                   "{}. Written bytes {}. Curr disk offset {}. Curr "
                   "write bytes {}",
                   vol_path_, err.message(), disk_offset_, data_size_,
                   written_size_, aio_data_.offs_, aio_data_.size_);
        cb_on_end_(false);
    }
}

void task_md_sync::setup_write() noexcept
{
    constexpr bytes64_t max_write_size = metadata_sync_size;
    const auto written_size = written_size_;
    const bytes32_t cur_write_size =
        std::min(data_size_ - written_size, max_write_size);
    X3ME_ASSERT((cur_write_size % store_block_size) == 0,
                "We do writes only in store_block_size blocks");

    aio_data_.buf_  = raw_data_.get() + written_size;
    aio_data_.offs_ = disk_offset_ + written_size;
    aio_data_.size_ = cur_write_size;

    XLOG_DEBUG(disk_tag,
               "Asynchronous metadata sync for cache "
               "FS for volume '{}'. Curr disk offset {} bytes. Curr write size "
               "{} bytes. Written bytes {}",
               vol_path_, aio_data_.offs_, cur_write_size, written_size);
}

} // namespace detail
} // namespace cache
