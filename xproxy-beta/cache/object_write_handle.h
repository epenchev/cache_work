#pragma once

#include "aio_task.h"
#include "cache_fs_ops_fwds.h"
#include "handler_buffers.h"
#include "range.h"
#include "read_buffers.h"
#include "write_transaction.h"
#include "frag_write_buff.h"

namespace cache
{
namespace detail
{

using write_handler_t = std::function<void(const err_code_t&, bytes32_t)>;

class object_write_handle final : public aio_task
{
    cache_fs_ops_ptr_t fs_ops_;

    using user_data_t = handler_buffers<write_handler_t, read_buffers>;
    // We don't expect any contention over these data. Thus spin lock is used.
    using user_data_sync_t =
        x3me::thread::synchronized<user_data_t, x3me::thread::spin_lock>;

    user_data_sync_t user_data_;

    // These data members are used only from the AIO write thread currently.
    // They don't need any locking.
    write_transaction wtrans_;
    // TODO A possible optimization here is to use io_buffer with multiple
    // blocks, instead of this single chunked buffer. It could lead to
    // smaller memory consumption and less fragmentation, because this single
    // chunked buffer every time is with different size.
    frag_write_buff wbuffer_;

    // We need to keep track of the processed bytes, because we may need
    // to skip some bytes from the beginning and from the end of the
    // original object data, if we already have them i.e. if the
    // transaction range is different than the object data range.
    bytes64_t processed_bytes_ = 0;

    // The write transaction range could be shorter than the actual one
    // one we want to skip some bytes that we already have
    // from the beginning and from the end.
    const range actual_rng_;

    enum struct state : uint32_t // Could be smaller.
    {
        running,
        close,
        closed,
        service_stopped,
    };
    std::atomic<state> state_{state::running};

public:
    object_write_handle(const cache_fs_ops_ptr_t& fso,
                        const range& actual_rng,
                        write_transaction&& wtrans) noexcept;
    ~object_write_handle() noexcept;

    // The async_write and async_close must be used from single thread only
    void async_write(buffers&& bufs, write_handler_t&& h) noexcept;

    // The user of this class must ensure that the object is not used
    // after a call to async_close
    void async_close() noexcept;

private:
    // This task/handle doesn't actually do any IO operations, because
    // it writes the buffers to the aggregate writer functionality.
    // So the operation is always execute only.
    aio_op operation() const noexcept final { return aio_op::exec; }

    void exec() noexcept final;

    non_owner_ptr_t<const aio_data> on_begin_io_op() noexcept final;
    void on_end_io_op(const err_code_t&) noexcept final;

    void service_stopped() noexcept final;

private:
    bool try_write_all() noexcept;
    void do_final_write() noexcept;
    frag_write_buff allocate_wbuff(bytes64_t full_exp_len) const noexcept;
    template <typename Err> // Avoid inclusion of cache_error.h
    void try_fire_error(Err err) noexcept;
};

using object_whandle_ptr_t = aio_task_ptr_t<object_write_handle>;

} // namespace detail
} // namespace cache
