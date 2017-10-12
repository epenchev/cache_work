#pragma once

namespace cache
{
namespace detail
{
struct aio_data;

enum struct aio_op : uint8_t
{
    exec,
    read,
    write,
};

using list_hook_t = boost::intrusive::list_base_hook<
    boost::intrusive::link_mode<boost::intrusive::safe_link>>;

////////////////////////////////////////////////////////////////////////////////

class aio_task : public list_hook_t
{
private:
    friend void intrusive_ptr_add_ref(aio_task*) noexcept;
    friend void intrusive_ptr_release(aio_task*) noexcept;
    std::atomic_uint ref_cnt_{0};

public:
    aio_task() noexcept {}
    // Note that the destructor of an aio_task can be called from any thread -
    // either from the given networking thread or from some of the AIO threads.
    // Thus don't make operations on global shared resources there unless
    // you are 100% sure what you are doing.
    virtual ~aio_task() noexcept {}

    aio_task(const aio_task&) = delete;
    aio_task& operator=(const aio_task&) = delete;
    aio_task(aio_task&&) = delete;
    aio_task& operator=(aio_task&&) = delete;

    virtual aio_op operation() const noexcept = 0;

    virtual void exec() noexcept = 0;

    // Why the IO is done in this way and outside of the task?
    // It's designed in this way because it'll allow easy change to
    // the native Linux AIO when needed. Only the aio_service functionality
    // will need to be changed.
    // Returning nullptr from here means that the task doesn't want anymore
    // to perform io_operation.
    virtual non_owner_ptr_t<const aio_data> on_begin_io_op() noexcept = 0;
    virtual void on_end_io_op(const err_code_t& err) noexcept = 0;

    virtual void service_stopped() noexcept = 0;

    uint32_t use_count() const noexcept { return ref_cnt_; }
};

template <typename AioTask>
using aio_task_ptr_t = boost::intrusive_ptr<AioTask>;

// Logging helper. It ensures that all types all derived tasks always
// log the pointer to the base.
// Thus the task can be traced through all systems if needed.
inline const void* log_ptr(const aio_task* t) noexcept
{
    return t;
}

////////////////////////////////////////////////////////////////////////////////

// These two are needed for boost::intrusive_ptr to work
inline void intrusive_ptr_add_ref(aio_task* p) noexcept
{
    ++p->ref_cnt_;
}

inline void intrusive_ptr_release(aio_task* p) noexcept
{
    if (--p->ref_cnt_ == 0)
        delete p;
}

template <typename AioTask, typename... Args>
inline aio_task_ptr_t<AioTask> make_aio_task(Args&&... args) noexcept
{
    return new AioTask(std::forward<Args>(args)...);
}

} // namespace detail
} // namespace cache
