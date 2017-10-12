#pragma once

namespace xutils
{

// This utility class may be useful when you need a reference counted object
// but you don't want/need to use the shared_ptr functionality because of it
// heavier/atomic reference counting.
// An object is supposed to be created only on the heap and thus most of
template <typename Data>
struct ref_counted : boost::intrusive_ref_counter<ref_counted<Data>,
                                                  boost::thread_unsafe_counter>
{
public:
    Data data_;

private:
    template <typename D, typename... Args>
    friend boost::intrusive_ptr<ref_counted<D>> make_ref_counted(Args&&...);

    template <typename... Args>
    explicit ref_counted(Args&&... args)
        : data_{std::forward<Args>(args)...}
    {
    }

public:
    ~ref_counted() noexcept = default;

    ref_counted(const ref_counted&) = delete;
    ref_counted& operator=(const ref_counted&) = delete;
    ref_counted(ref_counted&&) = delete;
    ref_counted& operator=(ref_counted&&) = delete;
};

template <typename Data, typename... Args>
boost::intrusive_ptr<ref_counted<Data>> make_ref_counted(Args&&... args)
{
    return new (std::nothrow) ref_counted<Data>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
boost::intrusive_ptr<T> make_intrusive(Args&&... args)
{
    return new (std::nothrow) T(std::forward<Args>(args)...);
}

} // namespace xutils
