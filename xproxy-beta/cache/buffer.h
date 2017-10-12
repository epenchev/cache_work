#pragma once

namespace cache
{
namespace detail
{

class buffers
{
protected:
    // Optimize (skip heap allocation) for the single buffer case.
    // Waste 16 bytes if we are not in the single buffer case.
    boost::container::small_vector<iovec, 2> data_;

public:
    buffers() noexcept {}
    ~buffers() noexcept = default;

    buffers(buffers&& rhs) noexcept : data_(std::move(rhs.data_)) {}
    buffers& operator=(buffers&& rhs) noexcept
    {
        data_ = std::move(rhs.data_);
        return *this;
    }

    void swap(buffers& rhs) noexcept { data_.swap(rhs.data_); }

protected:
    buffers(void* data, size_t size) noexcept : data_(1, iovec{data, size}) {}

    template <typename Buff>
    buffers(std::initializer_list<Buff> rhs) noexcept;
    template <typename Buff>
    buffers& operator=(std::initializer_list<Buff> rhs) noexcept;

    buffers(const buffers&) = delete;
    buffers& operator=(const buffers&) = delete;

    void emplace_back(void* data, size_t size) noexcept
    {
        data_.push_back(iovec{data, size});
    }

public:
    using data_t = x3me::mem_utils::array_view<iovec>;
    data_t data() noexcept
    {
        return x3me::mem_utils::make_array_view(data_.data(), data_.size());
    }

    void clear() noexcept { return data_.clear(); }

    auto size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
};

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

struct const_buffer;

struct mutable_buffer : public detail::buffers
{
    mutable_buffer(void* data, size_t size) noexcept : buffers{data, size} {}

    mutable_buffer(mutable_buffer&&) noexcept = default;
    mutable_buffer& operator=(mutable_buffer&&) noexcept = default;
};

struct const_buffer : public detail::buffers
{
    const_buffer(const void* data, size_t size) noexcept
        : buffers{const_cast<void*>(data), size}
    {
    }

    const_buffer(mutable_buffer&& rhs) noexcept
        : detail::buffers(std::move(rhs))
    {
    }
    const_buffer& operator=(mutable_buffer&& rhs) noexcept
    {
        detail::buffers::operator=(std::move(rhs));
        return *this;
    }

    const_buffer(const_buffer&&) noexcept = default;
    const_buffer& operator=(const_buffer&&) noexcept = default;
};

////////////////////////////////////////////////////////////////////////////////

struct const_buffers;

struct mutable_buffers : public detail::buffers
{
    using buffers::buffers;
    using buffers::operator=;

    mutable_buffers() noexcept {}

    // This constructor doesn't make sense for multiple buffers
    mutable_buffers(void*, size_t) = delete;
    // Moving mutable_buffers to const_buffers is allowed,
    // but the opposite is error prone and thus disabled.
    mutable_buffers(const_buffers&&) = delete;
    mutable_buffers& operator=(const_buffers&&) = delete;
    mutable_buffers(std::initializer_list<const_buffer>) = delete;
    mutable_buffers& operator=(std::initializer_list<const_buffer>) = delete;

    void emplace_back(void* data, size_t size) noexcept
    {
        buffers::emplace_back(data, size);
    }
};

struct const_buffers : public detail::buffers
{
    using buffers::buffers;
    using buffers::operator=;

    // This constructor doesn't make sense for multiple buffers
    const_buffers(void*, size_t) = delete;

    const_buffers() noexcept {}

    const_buffers(mutable_buffers&& rhs) noexcept
        : detail::buffers(std::move(rhs))
    {
    }
    const_buffers& operator=(mutable_buffers&& rhs) noexcept
    {
        detail::buffers::operator=(std::move(rhs));
        return *this;
    }

    void emplace_back(const void* data, size_t size) noexcept
    {
        buffers::emplace_back(const_cast<void*>(data), size);
    }
};

////////////////////////////////////////////////////////////////////////////////

inline mutable_buffer buffer(void* data, size_t size) noexcept
{
    return mutable_buffer(data, size);
}

inline const_buffer buffer(const void* data, size_t size) noexcept
{
    return const_buffer(data, size);
}

template <typename Container>
inline mutable_buffer buffer(Container& cont) noexcept
{
    using namespace x3me;
    // Must be C-Array, std::array, std::vector, etc with byte data
    static_assert(sizeof(*utils::data(cont)) == 1,
                  "Must be a container with byte data");
    return mutable_buffer(utils::data(cont), utils::size(cont));
}

template <typename Container>
inline const_buffer buffer(const Container& cont) noexcept
{
    using namespace x3me;
    // Must be C-Array, std::array, std::vector, etc with byte data
    static_assert(sizeof(*utils::data(cont)) == 1,
                  "Must be a container with byte data");
    return const_buffer(utils::data(cont), utils::size(cont));
}

} // namespace cache
