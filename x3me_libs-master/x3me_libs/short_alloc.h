#pragma once

#include <cstddef>
#include <cassert>

namespace x3me
{
namespace mem
{

// The GCC 4.8.2 still doesn't provide std::max_align_t
// template <size_t N, size_t Alignment = alignof(std::max_align_t)>
template <size_t N, size_t Alignment = 8>
class stack_arena
{
    alignas(Alignment) char buf_[N];
    char* ptr_;

public:
    static constexpr auto alignment = Alignment;

public:
    stack_arena() noexcept : ptr_(buf_) {}
    ~stack_arena() { ptr_ = nullptr; }

    stack_arena(const stack_arena&) = delete;
    stack_arena& operator=(const stack_arena&) = delete;
    stack_arena(stack_arena&&) = delete;
    stack_arena& operator=(stack_arena&&) = delete;

    char* allocate(size_t n) noexcept
    {
        assert(pointer_in_buffer(ptr_) &&
               "short_alloc has outlived stack_arena");
        const ptrdiff_t aligned_n = align_up(n);
        if ((buf_ + N - ptr_) >= aligned_n)
        {
            char* r = ptr_;
            ptr_ += aligned_n;
            return r;
        }
        return static_cast<char*>(::operator new(n));
    }

    void deallocate(char* p, size_t n) noexcept
    {
        assert(pointer_in_buffer(ptr_) &&
               "short_alloc has outlived stack_arena");
        if (pointer_in_buffer(p))
        {
            n = align_up(n);
            if ((p + n) == ptr_)
                ptr_ = p;
        }
        else
        {
            ::operator delete(p);
        }
    }

    static constexpr size_t size() noexcept { return N; }
    size_t used() const noexcept { return static_cast<size_t>(ptr_ - buf_); }
    void reset() noexcept { ptr_ = buf_; }

private:
    bool pointer_in_buffer(char* p) const noexcept
    {
        return buf_ <= p && p <= (buf_ + N);
    }

    static size_t align_up(size_t n) noexcept
    {
        return (n + (alignment - 1)) & ~(alignment - 1);
    }
};

////////////////////////////////////////////////////////////////////////////////

// The GCC 4.8.2 still doesn't provide std::max_align_t
// template <size_t N, size_t Alignment = alignof(std::max_align_t)>
template <size_t Alignment = 8>
class heap_arena
{
    char* buf_;
    char* ptr_;
    const size_t size_;

public:
    static constexpr auto alignment = Alignment;

public:
    explicit heap_arena(size_t size)
        : buf_(static_cast<char*>(::operator new(size)))
        , ptr_(buf_)
        , size_(size)
    {
    }
    ~heap_arena()
    {
        ::operator delete(buf_);
        buf_ = nullptr;
    }

    heap_arena(const heap_arena&) = delete;
    heap_arena& operator=(const heap_arena&) = delete;
    heap_arena(heap_arena&&) = delete;
    heap_arena& operator=(heap_arena&&) = delete;

    char* allocate(size_t n)
    {
        assert(buf_ && "short_alloc has outlived heap_arena");
        const ptrdiff_t aligned_n = align_up(n);
        if ((buf_ + size_ - ptr_) >= aligned_n)
        {
            char* r = ptr_;
            ptr_ += aligned_n;
            return r;
        }
        return static_cast<char*>(::operator new(n));
    }

    void deallocate(char* p, size_t n) noexcept
    {
        assert(buf_ && "short_alloc has outlived heap_arena");
        if (buf_ <= p && p <= (buf_ + size_))
        {
            n = align_up(n);
            if ((p + n) == ptr_)
                ptr_ = p;
        }
        else
            ::operator delete(p);
    }

    size_t size() const noexcept { return size_; }
    size_t used() const noexcept { return static_cast<size_t>(ptr_ - buf_); }
    void reset() noexcept { ptr_ = buf_; }

private:
    static size_t align_up(size_t n) noexcept
    {
        return (n + (alignment - 1)) & ~(alignment - 1);
    }
};

////////////////////////////////////////////////////////////////////////////////

template <typename T, typename Arena>
class short_alloc
{
    Arena* a_ = nullptr;

public:
    // This boilerplate shouldn't be needed for C++11, but there
    // seems to be a bug in the GCC standard library.
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;

    using value_type                = T;
    using arena_type                = Arena;
    static constexpr auto alignment = Arena::alignment;

public:
    template <typename U>
    struct rebind
    {
        using other = short_alloc<U, Arena>;
    };

    // The default constructor shouldn't be needed according to the
    // C++11 standard, but the GCC standard library currently doesn't
    // conform to it.
    short_alloc() noexcept = default;
    explicit short_alloc(Arena& a) noexcept : a_(&a) {}
    template <typename U>
    short_alloc(const short_alloc<U, Arena>& a) noexcept : a_(a.a_)
    {
    }
    short_alloc(const short_alloc&) noexcept = default;
    short_alloc& operator=(const short_alloc&) = delete;
    ~short_alloc()
    {
        static_assert(alignof(T) <= alignment,
                      "Type and arena alignment mismatch");
    }

    T* allocate(size_t n)
    {
        return reinterpret_cast<T*>(a_->allocate(n * sizeof(T)));
    }
    void deallocate(T* p, size_t n) noexcept
    {
        a_->deallocate(reinterpret_cast<char*>(p), n * sizeof(T));
    }

    // The GCC's library still needs these. They shouldn't be needed
    // according to the C++11 standard.
    template <class U, class... Args>
    void construct(U* p, Args&&... args)
    {
        ::new (p) U(std::forward<Args>(args)...);
    }
    template <class U>
    void destroy(U* p) noexcept
    {
        p->~U();
    }

    template <typename T1, typename T2, typename A>
    friend bool operator==(const short_alloc<T1, A>& x,
                           const short_alloc<T2, A>& y) noexcept;

    template <typename U, typename A>
    friend class short_alloc;
};

////////////////////////////////////////////////////////////////////////////////

template <typename T1, typename T2, typename A>
bool operator==(const short_alloc<T1, A>& x,
                const short_alloc<T2, A>& y) noexcept
{
    return x.a_ == y.a_;
}

template <typename T1, typename T2, typename A>
inline bool operator!=(const short_alloc<T1, A>& x,
                       const short_alloc<T2, A>& y) noexcept
{
    return !(x == y);
}

} // namespace mem
} // namespace x3me
