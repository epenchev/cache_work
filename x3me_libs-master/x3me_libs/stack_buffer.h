#pragma once

#include <cassert>
#include <cstring>
#include <iterator>
#include <type_traits>

namespace x3me
{
namespace mem_utils
{

template <typename T, size_t Capacity>
class stack_buffer
{
    static_assert(std::is_trivial<T>::value, "");

public:
    enum
    {
        scapacity = Capacity
    };

public:
    typedef T value_type;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

private:
    size_type size_;
    value_type buff_[scapacity];

public:
    stack_buffer() noexcept : size_(0) {}

    stack_buffer(const stack_buffer& rhs) noexcept : size_(rhs.size_)
    {
        std::memcpy(buff_, rhs.buff_, rhs.size_);
    }

    stack_buffer& operator=(const stack_buffer& rhs) noexcept
    {
        if (this != &rhs)
        {
            size_ = rhs.size_;
            std::memcpy(buff_, rhs.buff_, rhs.size_);
        }
        return *this;
    }

    stack_buffer(stack_buffer&&) = delete;
    stack_buffer& operator=(stack_buffer&&) = delete;

    ~stack_buffer() noexcept = default;

    stack_buffer(const_pointer p, size_type size) noexcept { assign(p, size); }

    void assign(const_pointer p, size_type size) noexcept
    {
        assert(p && (scapacity >= size));
        std::memcpy(buff_, p, size);
        size_ = size;
    }

    void append(const_pointer p, size_type size) noexcept
    {
        assert(p && (free_size() >= size));
        std::memcpy(buff_ + size_, p, size);
        size_ += size;
    }

    void mem_move(size_type offset) noexcept
    {
        assert(offset <= size_); // Implies offset < scapacity also
        auto rem_size = size_ - offset;
        std::memmove(buff_, buff_ + offset, rem_size);
        size_ = rem_size;
    }

    void clear() noexcept { size_ = 0; }

    const_iterator cbegin() const noexcept
    {
        assert(!empty());
        return buff_;
    }
    const_iterator cend() const noexcept
    {
        assert(!empty());
        return (buff_ + size_);
    }

    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator end() const noexcept { return cend(); }
    iterator begin() noexcept
    {
        assert(!empty());
        return buff_;
    }
    iterator end() noexcept
    {
        assert(!empty());
        return (buff_ + size_);
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }
    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }
    const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    const_reverse_iterator rend() const noexcept { return crend(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    // To express that you have no data yet
    pointer buffer() noexcept { return buff_; }
    const_pointer buffer() const noexcept { return buff_; }

    pointer free_space() noexcept
    {
        assert(scapacity > size_);
        return (buff_ + size_);
    }

    pointer data() noexcept { return buff_; }

    const_pointer data() const noexcept { return buff_; }

    pointer data_at(size_type offset) noexcept
    {
        assert(size_ > offset);
        return (buff_ + offset);
    }

    const_pointer data_at(size_type offset) const noexcept
    {
        assert(size_ > offset);
        return (buff_ + offset);
    }

    reference operator[](size_type i) noexcept { return *data_at(i); }

    const_reference operator[](size_type i) const noexcept
    {
        return *data_at(i);
    }

    void set_size(size_type size) noexcept
    {
        assert(scapacity >= size);
        size_ = size;
    }

    void increase_size(size_type size) noexcept { set_size(size_ + size); }

    static constexpr size_type capacity() noexcept { return scapacity; }

    size_type size() const noexcept { return size_; }

    size_type free_size() const noexcept { return (scapacity - size_); }

    bool empty() const noexcept { return (size_ == 0); }

    bool full() const noexcept { return (size_ == scapacity); }
};

} // namespace mem_utils
} // namespace x3me
