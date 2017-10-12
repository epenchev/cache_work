#pragma once

#include <type_traits>
#include <utility>

#include "mpl.h"

namespace x3me
{
namespace utils
{

template <typename T, size_t Size, size_t Align>
class pimpl
{
    using storage_t = typename std::aligned_storage<Size, Align>::type;

    storage_t storage_;

public:
    using type_t = T;

public:
    pimpl() { new (get()) T; }

    template <typename... Args>
    pimpl(Args&&... args)
    {
        new (get()) T(std::forward<Args>(args)...);
    }

    pimpl(const pimpl& rhs) { new (get()) T(*rhs.get()); }

    pimpl(pimpl&& rhs) { new (get()) T(std::move(*rhs.get())); }

    pimpl& operator=(const pimpl& rhs)
    {
        *get() = *rhs.get();
        return *this;
    }

    pimpl& operator=(pimpl&& rhs)
    {
        *get() = std::move(*rhs.get());
        return *this;
    }

    ~pimpl()
    {
        enum
        {
            valign = std::alignment_of<T>::value
        };
        static_assert(
            mpl::num_check<(sizeof(T) == Size), sizeof(T), Size>::value,
            "Incorrect Size");
        static_assert(mpl::num_check<(valign == Align), valign, Align>::value,
                      "Incorrect Align");
        get()->~T();
    }

    T* get() { return static_cast<T*>(static_cast<void*>(&storage_)); }

    const T* get() const
    {
        return static_cast<const T*>(static_cast<const void*>(&storage_));
    }

    T* operator->() { return get(); }

    const T* operator->() const { return get(); }

    T& operator*() { return *get(); }

    const T& operator*() const { return *get(); }
};

} // namespace utils
} // namespace x3me
