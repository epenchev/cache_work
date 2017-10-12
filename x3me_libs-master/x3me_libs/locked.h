#pragma once

#include <type_traits>

namespace x3me
{
namespace thread
{
namespace detail
{

struct unique_locked_policy
{
    static constexpr bool const_data = false; // Allows non const data

    template <typename Mutex>
    static void lock(Mutex& m) noexcept(noexcept(m.lock()))
    {
        m.lock();
    }
    template <typename Mutex>
    static void unlock(Mutex& m) noexcept
    {
        m.unlock();
    }
};

struct shared_locked_policy
{
    static constexpr bool const_data = true; // Allows only const data

    template <typename Mutex>
    static void lock(Mutex& m) noexcept(noexcept(m.lock_shared()))
    {
        m.lock_shared();
    }
    template <typename Mutex>
    static void unlock(Mutex& m) noexcept
    {
        m.unlock_shared();
    }
};

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

// Useful when we want to return a locked data from a getter function.
template <typename Data, typename Mutex, typename Policy>
class locked
{
public:
    using data_t = typename std::conditional<
        Policy::const_data, typename std::add_const<Data>::type, Data>::type;
    using mutex_t = Mutex;

private:
    data_t* data_ = nullptr;
    mutex_t* mut_ = nullptr;

public:
    locked() noexcept = default;
    locked(data_t& data, mutex_t& mut) noexcept(noexcept(Policy::lock(*mut_)))
        : data_(&data), mut_(&mut)
    {
        Policy::lock(*mut_);
    }
    locked(locked&& rhs) noexcept : data_(rhs.data_), mut_(rhs.mut_)
    {
        rhs.data_ = nullptr;
        rhs.mut_  = nullptr;
    }
    locked& operator=(locked&& rhs) noexcept
    {
        if (mut_)
            Policy::unlock(*mut_);
        data_     = rhs.data_;
        mut_      = rhs.mut_;
        rhs.data_ = nullptr;
        rhs.mut_  = nullptr;
        return *this;
    }
    ~locked() noexcept
    {
        if (mut_)
            Policy::unlock(*mut_);
    }

    locked(const locked&) = delete;
    locked& operator=(const locked&) = delete;

    explicit operator bool() const noexcept { return !!data_; }

    data_t* operator->() const noexcept { return data_; }
    data_t& operator*() const noexcept { return *data_; }
};

////////////////////////////////////////////////////////////////////////////////

template <typename Data, typename Mutex>
using unique_locked = locked<Data, Mutex, detail::unique_locked_policy>;

template <typename Data, typename Mutex>
using shared_locked = locked<Data, Mutex, detail::shared_locked_policy>;

template <typename Data, typename Mutex>
auto make_unique_locked(Data& data, Mutex& mut)
{
    return unique_locked<Data, Mutex>(data, mut);
}

template <typename Data, typename Mutex>
auto make_shared_locked(Data& data, Mutex& mut)
{
    return shared_locked<Data, Mutex>(data, mut);
}

} // namespace thread
} // namespace x3me
