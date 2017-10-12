#pragma once

#include "aio_data.h"
#include "aio_task.h"
#include "cache_fs_ops_fwds.h"
#include "aligned_data_ptr.h"
#include "handler_buffers.h"
#include "range_elem.h"
#include "read_transaction.h"
#include "write_buffers.h"

namespace cache
{
namespace detail
{
class buffers;

using read_handler_t  = std::function<void(const err_code_t&, bytes32_t)>;
using close_handler_t = std::function<void(const err_code_t&)>;

class object_read_handle final : public aio_task
{
    cache_fs_ops_ptr_t fs_ops_;

    class user_data
    {
    public:
        using uh_buffers_t = handler_buffers<read_handler_t, write_buffers>;

        using close_handler_ptr_t = std::unique_ptr<close_handler_t>;

    private:
        uh_buffers_t uh_buffers_;
        // The close handler is expected to be used in very few cases and thus
        // I want it to take as little space as possible even at the expense of
        // additional heap allocation when needed.
        close_handler_ptr_t close_handler_;

    public:
        void set_uh_buffers(read_handler_t&& h, buffers&& wb) noexcept;
        void swap_uh_buffers(uh_buffers_t& rhs) noexcept;

        void set_close_handler(close_handler_ptr_t&& h) noexcept;
        close_handler_ptr_t release_close_handler() noexcept;
    };
    // We don't expect any contention over these data. Thus spin lock is used.
    using user_data_sync_t =
        x3me::thread::synchronized<user_data, x3me::thread::spin_lock>;

    user_data_sync_t user_data_;

    // All other members except the user_data are accessed from the read threads
    // but never concurrently. The task logic guarantees that the multiple
    // invocations of the same task are always done sequentially.
    // One tiny exception is the read_transaction where it's obj_key is used
    // for logging from both the read_threads and the upper layer threads, but
    // the access to the obj_key is always read_only.

    read_transaction rtrans_;

    aio_data aio_data_;

    range_elem curr_rng_ = make_zero_range_elem();

    aligned_data_ptr_t frag_read_buff_;
    bytes32_t read_buff_size_ = 0;

    enum struct state : uint16_t // Could be smaller.
    {
        running,
        close,
        closed,
        service_stopped,
    };
    std::atomic<state> state_{state::running};

    // Unfortunately we need to serialize on_begin/end_io_op because of the
    // close functionality. Calling close may cause the close task to be
    // executed concurrently with the current executing task. Thus we use
    // the spin_lock::try_lock to acquire access to the critical section,
    // otherwise we repost the second task on the queue again.
    // Maybe in the future we'll add something similar to the ASIO strands
    // to the cache::aio_service functionality and this way we'll be able
    // to serialize single read_handle. I can't add it now because it needs
    // some scheduling logic because simple round robin for the read_handles
    // won't work well.
    x3me::thread::spin_lock serializator_;

    bool vol_mutex_locked_ = false;

public:
    object_read_handle(const cache_fs_ops_ptr_t& fso,
                       read_transaction&& rtrans) noexcept;
    ~object_read_handle() final;

    // The async_read and async_close must be used from single thread only
    void async_read(buffers&& bufs, read_handler_t&& h) noexcept;

    void async_close(close_handler_t&& h) noexcept;

    // The user of this class must ensure that the object is not used
    // after a call to async_close
    void async_close() noexcept;

private:
    aio_op operation() const noexcept final { return aio_op::read; }

    void exec() noexcept final;

    non_owner_ptr_t<const aio_data> on_begin_io_op() noexcept final;

    struct begin_io_op_res
    {
        non_owner_ptr_t<const aio_data> aio_data_;
        bool try_read_mem_;
    };
    begin_io_op_res begin_io_op() noexcept;

    void on_end_io_op(const err_code_t& err) noexcept final;

    void service_stopped() noexcept final;

#ifdef X3ME_TEST
public:
#endif
    // Needs to be specifically unit tested because a few edge cases.
    // Returns the offset and the length of the range to be copied.
    static std::pair<bytes32_t, bytes32_t>
    calc_copy_rng(const read_transaction& rtrans,
                  const range_elem& rng) noexcept;

private:
    enum struct read_res
    {
        all_read,
        end_of_buf,
        aborted,
    };
    read_res try_read_all_from_mem_buff() noexcept;
    bool check_read_data() const noexcept;
    void read_handle_done() noexcept;
    template <typename Err> // Avoid inclusion of cache_error.h
    void try_fire_error(Err err) noexcept;
    template <typename Err> // Avoid inclusion of cache_error.h
    void try_fire_closed(Err err) noexcept;
    void report_disk_error(const err_code_t& err,
                           bytes64_t off,
                           bytes32_t len) noexcept;
};

using object_rhandle_ptr_t = aio_task_ptr_t<object_read_handle>;

} // namespace detail
} // namespace cache
