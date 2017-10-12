#pragma once

#include "aio_task.h"
#include "aio_data.h"
#include "aligned_data_ptr.h"
#include "cache_fs_ops_fwds.h"

namespace cache
{
namespace detail
{

class task_md_sync final : public aio_task
{
    using on_end_cb_t = std::function<void(bool)>;

    non_owner_ptr_t<cache_fs_ops> fs_ops_;
    aio_data aio_data_;
    on_end_cb_t cb_on_end_;
    aligned_data_ptr_t raw_data_;
    bytes64_t written_size_;
    const bytes64_t data_size_;
    const bytes64_t disk_offset_;
    const boost::container::string& vol_path_;

public:
    task_md_sync(non_owner_ptr_t<cache_fs_ops> fso,
                 aligned_data_ptr_t&& raw_data, bytes64_t data_size,
                 bytes64_t disk_offset, const on_end_cb_t& cb_on_end) noexcept;

private:
    aio_op operation() const noexcept final { return aio_op::write; }

    void exec() noexcept final;

    non_owner_ptr_t<const aio_data> on_begin_io_op() noexcept final
    {
        return &aio_data_;
    }

    void on_end_io_op(const err_code_t& err) noexcept final;

    // We don't need to do anything here
    void service_stopped() noexcept final {}

private:
    void setup_write() noexcept;
};

} // namespace detail
} // namespace cache
