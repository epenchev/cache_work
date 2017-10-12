#pragma once

#include <stdlib.h>

#include <streambuf>
#include <ostream>

namespace x3me
{
namespace utils
{

template <size_t BufferSize>
class string_builder
{
private:
    template <size_t Size>
    class buffer : public std::streambuf
    {
    public:
        using size_type = size_t;

    private:
        union
        {
            struct
            {
                char* heap_mem_;
                size_type heap_mem_size_;
            };
            char_type stack_mem_[Size];
        };
        bool uses_heap_;

    public:
        buffer() noexcept : uses_heap_(false)
        {
            setp(stack_mem_, stack_mem_ + Size);
        }

        ~buffer() noexcept
        {
            if (uses_heap_)
                free(heap_mem_);
        }

        const char_type* cbegin() const noexcept { return pbase(); }

        const char_type* cend() const noexcept { return pptr(); }

        bool uses_heap() const noexcept { return uses_heap_; }

    protected:
        virtual int_type overflow(int_type c) noexcept final
        {
            size_type alloc_size = 0;
            size_type curr_size  = 0;
            char_type* old_mem   = nullptr;

            if (!uses_heap_)
            {
                alloc_size = (Size + 1) * 2;
                curr_size  = Size;
                old_mem    = stack_mem_;
            }
            else
            {
                alloc_size = heap_mem_size_ * 2;
                curr_size  = heap_mem_size_;
                old_mem    = heap_mem_;
            }

            auto new_mem = static_cast<char_type*>(malloc(alloc_size));

            memcpy(new_mem, old_mem, curr_size);
            new_mem[curr_size] = char_type(c);

            if (uses_heap_)
                free(heap_mem_);
            else
                uses_heap_ = true;

            // redirect the pointer
            heap_mem_      = new_mem;
            heap_mem_size_ = alloc_size;
            // redirect streambuf pointers
            setp(heap_mem_, heap_mem_ + heap_mem_size_);
            pbump(int_type(curr_size) + 1);

            return traits_type::not_eof(c);
        }
    };

private:
    // The buffer has an additional byte for a boolean field
    // we give it BufferSize - 1 to preserve a good alignment
    static_assert(BufferSize > 1, "");
    using buffer_type = buffer<BufferSize - 1>;
    // The constructor of the strm_ depends on the buff_
    buffer_type buff_;
    std::ostream strm_;

public:
    using value_type = typename buffer_type::char_type;
    using size_type  = typename buffer_type::size_type;

public:
    string_builder() noexcept : strm_(&buff_) {}

    template <typename T>
    string_builder& operator<<(const T& value) noexcept
    {
        strm_ << value;
        return *this;
    }

    string_builder& operator<<(std::ostream& iomanip(std::ostream&)) noexcept
    {
        strm_ << iomanip;
        return *this;
    }

    const value_type* data() const noexcept // NB: non zero terminated
    {
        return static_cast<buffer_type*>(strm_.rdbuf())->cbegin();
    }

    size_type size() const noexcept
    {
        auto buff = static_cast<buffer_type*>(strm_.rdbuf());
        return (buff->cend() - buff->cbegin());
    }

    bool uses_heap() const noexcept
    {
        return static_cast<buffer_type*>(strm_.rdbuf())->uses_heap();
    }

    // Better construct string_ref/view(data, size) object
    // if you don't really need string to outlive the string_builder object.
    std::string to_string() const noexcept
    {
        auto buff = static_cast<buffer_type*>(strm_.rdbuf());
        return std::string(buff->cbegin(), buff->cend());
    }
};

#define STRING_BUILDERS(MACRO)                                                 \
    MACRO(32)                                                                  \
    MACRO(64)                                                                  \
    MACRO(128)                                                                 \
    MACRO(256)                                                                 \
    MACRO(512)                                                                 \
    MACRO(1024)                                                                \
    MACRO(2048)                                                                \
    MACRO(4096)                                                                \
    MACRO(8192)

#define STRING_BUILDERS_IT(buff_size)                                          \
    using string_builder_##buff_size = string_builder<buff_size>;

STRING_BUILDERS(STRING_BUILDERS_IT)

#undef STRING_BUILDERS_IT

} // namespace utils
// TODO Remove this namespace when migrate all apps
namespace utilities
{

template <size_t BufferSize>
using string_builder = utils::string_builder<BufferSize>;

#define STRING_BUILDERS_IT(buff_size)                                          \
    using string_builder_##buff_size = utils::string_builder<buff_size>;

STRING_BUILDERS(STRING_BUILDERS_IT)

#undef STRING_BUILDERS_IT

#undef STRING_BUILDERS

} // namespace utilities
} // namespace x3me
