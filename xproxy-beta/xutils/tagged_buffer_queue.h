#pragma once

namespace xutils
{

template <typename Tag>
class tagged_buffer;
template <typename Tag>
using tagged_buffer_ptr_t = std::unique_ptr<tagged_buffer<Tag>>;

namespace detail
{
using list_hook_t = boost::intrusive::list_base_hook<
    boost::intrusive::link_mode<boost::intrusive::safe_link>>;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

template <typename Tag>
class tagged_buffer : public detail::list_hook_t, public Tag
{
    template <typename... Args>
    tagged_buffer(void* buff, size_t size, Args&&... args)
        : Tag(buff, size, std::forward<Args>(args)...)
    {
    }

public:
    ~tagged_buffer() noexcept = default;

    tagged_buffer() = delete;
    tagged_buffer(const tagged_buffer&) = delete;
    tagged_buffer& operator=(const tagged_buffer&) = delete;
    tagged_buffer(tagged_buffer&&) = delete;
    tagged_buffer& operator=(tagged_buffer&&) = delete;

    static void operator delete(void* p) noexcept { free(p); }

    template <typename... Args>
    static auto create(size_t bufsize, Args&&... args) noexcept
    {
        using tagged_buffer_t = tagged_buffer<Tag>;
        tagged_buffer_t* ret  = nullptr;
        void* p               = malloc(sizeof(tagged_buffer_t) + bufsize);
        try
        {
            void* buf = static_cast<char*>(p) + sizeof(tagged_buffer_t);
            ret = new (p)
                tagged_buffer_t(buf, bufsize, std::forward<Args>(args)...);
            assert(ret == p);
        }
        catch (...)
        {
            free(p);
        }
        return tagged_buffer_ptr_t<Tag>(ret);
    }
};

////////////////////////////////////////////////////////////////////////////////

template <typename Tag>
class tagged_buffer_queue
{
    using impl_t =
        boost::intrusive::list<tagged_buffer<Tag>,
                               boost::intrusive::constant_time_size<true>>;

    impl_t impl_;

public:
    using value_type = tagged_buffer_ptr_t<Tag>;
    using size_type  = size_t;

public:
    tagged_buffer_queue() noexcept {}
    ~tagged_buffer_queue() noexcept
    {
        using deleter_t = std::default_delete<tagged_buffer<Tag>>;
        impl_.clear_and_dispose(deleter_t{});
    }

    tagged_buffer_queue(tagged_buffer_queue&& rhs) noexcept
        : impl_(std::move(rhs.impl_))
    {
    }
    tagged_buffer_queue& operator=(tagged_buffer_queue&& rhs) noexcept
    {
        impl_ = std::move(rhs.impl_);
        return *this;
    }

    tagged_buffer_queue(const tagged_buffer_queue&) = delete;
    tagged_buffer_queue& operator=(const tagged_buffer_queue&) = delete;

    void push(value_type&& v) noexcept { impl_.push_back(*v.release()); }

    template <typename... Args>
    void emplace(size_t bufsize, Args&&... args) noexcept
    {
        push(tagged_buffer<Tag>::create(bufsize, std::forward<Args>(args)...));
    }

    value_type pop() noexcept
    {
        value_type ret;
        if (!impl_.empty())
        {
            ret.reset(&impl_.front());
            impl_.pop_front();
        }
        return ret;
    }

    void swap(tagged_buffer_queue& rhs) noexcept { impl_.swap(rhs.impl_); }

    bool empty() const noexcept { return impl_.empty(); }

    size_type size() const noexcept { return impl_.size(); }
};

} // namespace xutils
