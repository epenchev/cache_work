#pragma once

// TODO Remove this functionality
namespace cache
{
namespace detail
{

struct volume_access_mutex : x3me::thread::shared_mutex
{
    void wait_previous_readers() noexcept
    {
        lock();
        unlock();
    }
};

} // namespace detail
} // namespace cache
