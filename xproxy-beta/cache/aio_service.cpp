#include "precompiled.h"
#include "aio_service.h"
#include "aio_task.h"
#include "aio_data.h"
#include "volume_fd.h"

namespace cache
{
namespace detail
{

aio_service::aio_service(volume_fd& vol_fd) noexcept : vol_fd_(vol_fd)
{
}

aio_service::~aio_service() noexcept
{
}

void aio_service::start(const boost::container::string& vol_path,
                        uint16_t num_threads) noexcept
{
    X3ME_ASSERT(num_threads >= min_num_threads,
                "Must have at least min_num_threads");
    X3ME_ASSERT(threads_.empty(), "Can't start aio_service more than once");

    // The label is created from the last max 4 letters of the volume path.
    // The volume path is usually /dev/sda, /dev/sdb or /dev/sdaa in the
    // worst case.
    const auto lbl = [](const boost::container::string& vol_path)
    {
        auto pos = vol_path.find_last_of('/');
        if (pos == std::string::npos)
            pos = 0; // Take from the beginning
        else
            pos += 1;
        const auto len = std::min(size_t(4), vol_path.size() - pos);
        return x3me::str_utils::stack_string<5>(&vol_path[pos], len);
    }(vol_path);

    threads_.reserve(num_threads);
    // We have one writer thread and N reader threads
    using x3me::sys_utils::set_this_thread_name;
    std::array<char, 16> name;
    ::snprintf(name.data(), name.size(), "xproxy_wr_%s", lbl.c_str());
    threads_.emplace_back([this, name]
                          {
                              set_this_thread_name(name.data());
                              process_queue(write_queue_, vol_fd_);
                          });
    for (uint16_t i = 1; i < num_threads; ++i)
    {
        ::snprintf(name.data(), name.size(), "xproxy_rd_%s", lbl.c_str());
        threads_.emplace_back([this, name]
                              {
                                  set_this_thread_name(name.data());
                                  process_queue(read_queue_, vol_fd_);
                              });
    }
}

void aio_service::stop()
{
    read_queue_.stop();
    write_queue_.stop();

    for (auto& t : threads_)
    {
        if (t.joinable())
            t.join();
    }
    // We don't support start after stop.
    // Thus we don't clear the threads_ here, so that we can assert on them.

    // The threads are stopped. It's safe to clear the queues.
    clear_queue_on_stop(read_queue_);
    clear_queue_on_stop(write_queue_);
}

////////////////////////////////////////////////////////////////////////////////

void aio_service::process_queue(aio_task_queue& queue, volume_fd& fd) noexcept
{
    // We have increased the task reference count when we pushed it to the
    // given queue. Now we have to decrease the task reference count
    // when we pop it from the queue. If this is the last reference to the
    // task it'll get destroyed. If the task self posted again, or
    // somebody holds the last reference from outside the task will continue
    // it's life.
    non_owner_ptr_t<aio_task> task;
    while ((task = queue.pop()))
    {
        switch (task->operation())
        {
        case aio_op::exec:
            task->exec();
            break;
        case aio_op::read:
            if (auto d = task->on_begin_io_op())
            {
                err_code_t err;
                fd.read(d->buf_, d->size_, d->offs_, err);
                task->on_end_io_op(err);
            }
            break;
        case aio_op::write:
            if (auto d = task->on_begin_io_op())
            {
                err_code_t err;
                fd.write(d->buf_, d->size_, d->offs_, err);
                task->on_end_io_op(err);
            }
            break;
        default:
            X3ME_ASSERT(false, "Missing switch case");
            break;
        }
        intrusive_ptr_release(task);
    }
}

void aio_service::push_front_task(owner_ptr_t<aio_task> t,
                                  aio_task_queue& queue) noexcept
{
    intrusive_ptr_add_ref(t);
    const bool r = queue.push_front(t);
    if (X3ME_UNLIKELY(!r))
    {
        t->service_stopped();
        intrusive_ptr_release(t);
    }
}

void aio_service::push_task(owner_ptr_t<aio_task> t,
                            aio_task_queue& queue) noexcept
{
    intrusive_ptr_add_ref(t);
    const bool r = queue.push_back(t);
    if (X3ME_UNLIKELY(!r))
    {
        t->service_stopped();
        intrusive_ptr_release(t);
    }
}

void aio_service::enqueue_task(owner_ptr_t<aio_task> t,
                               aio_task_queue& queue) noexcept
{
    intrusive_ptr_add_ref(t);
    const auto r = queue.enqueue(t);
    if (X3ME_UNLIKELY(r == aio_task_queue::enqueue_res::stopped))
    {
        t->service_stopped();
        intrusive_ptr_release(t);
    }
    else if (r == aio_task_queue::enqueue_res::skipped)
    {
        // We need to decrease back the reference count of the task if we can't
        // enqueue it, i.e. if it's already in the queue. It has already
        // increased reference count if it's already in the queue.
        intrusive_ptr_release(t);
    }
    else
    {
        X3ME_ASSERT(r == aio_task_queue::enqueue_res::enqueued,
                    "Missing if-else case");
    }
}

bool aio_service::cancel_task(non_owner_ptr_t<aio_task> t,
                              aio_task_queue& queue) noexcept
{
    auto task = queue.remove_task(t);
    if (task)
        intrusive_ptr_release(task);
    return !!task;
}

void aio_service::clear_queue_on_stop(aio_task_queue& queue) noexcept
{
    auto tasks = queue.release_all();
    for (auto& t : tasks)
        t.service_stopped();
    tasks.clear_and_dispose(&intrusive_ptr_release);
}

} // namespace detail
} // namespace cache
