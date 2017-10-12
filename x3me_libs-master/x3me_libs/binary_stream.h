#pragma once

#include <cassert>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

#include "convert.h"
#include "utils.h"

namespace x3me
{
namespace bin_utils
{

template <typename T>
class binary_stream;

////////////////////////////////////////////////////////////////////////////////

template <typename T>
class static_streambuf
{
    static_assert(
        std::is_same<typename std::make_unsigned<T>::type, uint8_t>::value,
        "Works only with signed/unsigned bytes");

    T *buff_;
    size_t size_;
    const size_t capacity_;

public:
    using value_type = T;

public:
    static_streambuf(T *buff, size_t cap) noexcept : buff_(buff),
                                                     size_(0),
                                                     capacity_(cap)
    {
    }
    template <typename Cont>
    static_streambuf(Cont &c) noexcept
        : static_streambuf(utils::data(c), utils::size(c))
    {
    }
    ~static_streambuf() noexcept = default;

    static_streambuf() = delete;
    static_streambuf(const static_streambuf &) = delete;
    static_streambuf &operator=(const static_streambuf &) = delete;
    static_streambuf(static_streambuf &&) = delete;
    static_streambuf &operator=(static_streambuf &&) = delete;

    const T *data() const noexcept { return buff_; }
    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return (size_ == 0); }

private:
    template <typename StreamBuf>
    friend class binary_stream;

    T *grab_buff(size_t size) noexcept
    {
        assert(size > 0);
        if ((size_ + size) <= capacity_)
        {
            auto p = buff_ + size_;
            size_ += size;
            return p;
        }
        assert(false); // The buffer is not big enough
        return nullptr;
    }
};

////////////////////////////////////////////////////////////////////////////////

template <typename T>
class dynamic_streambuf
{
    static_assert(
        std::is_same<typename std::make_unsigned<T>::type, uint8_t>::value,
        "Works only with signed/unsigned bytes");

public:
    struct buff
    {
        std::unique_ptr<T[]> buff_;
        size_t size_     = 0;
        size_t capacity_ = 0;
    };
    using value_type = T;

private:
    buff buff_;

public:
    dynamic_streambuf() noexcept = default;
    explicit dynamic_streambuf(size_t cap) noexcept
    {
        buff_.buff_.reset(new (std::nothrow) T[cap]);
        assert(buff_.buff_);
        buff_.capacity_ = cap;
    }
    ~dynamic_streambuf() noexcept = default;

    dynamic_streambuf(const dynamic_streambuf &) = delete;
    dynamic_streambuf &operator=(const dynamic_streambuf &) = delete;
    dynamic_streambuf(dynamic_streambuf &&) = delete;
    dynamic_streambuf &operator=(dynamic_streambuf &&) = delete;

    const T *data() const noexcept { return buff_.buff_.get(); }
    size_t size() const noexcept { return buff_.size_; }
    size_t capacity() const noexcept { return buff_.capacity_; }
    bool empty() const noexcept { return (buff_.size_ == 0); }

    buff release() noexcept
    {
        buff tmp = std::move(buff_);
        buff_    = buff{};
        return tmp;
    }

private:
    template <typename StreamBuf>
    friend class binary_stream;

    T *grab_buff(size_t size) noexcept
    {
        assert(size > 0);
        const auto old_size = buff_.size_;
        if ((old_size + size) > buff_.capacity_)
        {
            auto new_cap = std::max(old_size + size, old_size * 2);
            auto p = new (std::nothrow) T[new_cap];
            assert(p);
            if (old_size > 0)
                std::memcpy(p, buff_.buff_.get(), old_size);
            buff_.buff_.reset(p);
            buff_.capacity_ = new_cap;
        }
        buff_.size_ = old_size + size;
        return buff_.buff_.get() + old_size;
    }
};

////////////////////////////////////////////////////////////////////////////////

template <typename StreamBuf>
class binary_stream
{
    StreamBuf &buf_;

    template <size_t S>
    struct size_type
    {
    };
    using size_1 = size_type<1>;
    using size_2 = size_type<2>;
    using size_4 = size_type<4>;
    using size_8 = size_type<8>;

public:
    using value_type = typename StreamBuf::value_type;

public:
    explicit binary_stream(StreamBuf &buf) noexcept : buf_(buf) {}
    ~binary_stream() noexcept = default;

    binary_stream() = delete;
    binary_stream(const binary_stream &) noexcept = default;
    binary_stream &operator=(const binary_stream &) noexcept = default;
    binary_stream(binary_stream &&) noexcept = default;
    binary_stream &operator=(binary_stream &&) noexcept = default;

    // Valid only for integer types
    // Writes the number in host order
    template <typename T>
    typename std::enable_if<std::numeric_limits<T>::is_integer, void>::type
    write(T v) noexcept
    {
        auto p = buf_.grab_buff(sizeof(v));
        std::memcpy(p, &v, sizeof(v));
    }

    void write8(uint8_t v) noexcept { write(v); }
    void write16(uint16_t v) noexcept { write(v); }
    void write32(uint32_t v) noexcept { write(v); }
    void write64(uint64_t v) noexcept { write(v); }

    // Valid only for integer types
    // Writes the number in network order
    template <typename T>
    typename std::enable_if<std::numeric_limits<T>::is_integer, void>::type
    write_net_order(T v) noexcept
    {
        write_net_num(v, size_type<sizeof(T)>{});
    }

    void write8_net_order(uint8_t v) noexcept { write_net_num(v, size_1{}); }
    void write16_net_order(uint16_t v) noexcept { write_net_num(v, size_2{}); }
    void write32_net_order(uint32_t v) noexcept { write_net_num(v, size_4{}); }
    void write64_net_order(uint64_t v) noexcept { write_net_num(v, size_8{}); }

    // Writes the data in the provided order
    void write(const value_type *v, size_t size) noexcept
    {
        auto p = buf_.grab_buff(size);
        std::memcpy(p, v, size);
    }

private:
    template <typename T>
    void write_net_num(T v, size_1) noexcept
    {
        auto p = buf_.grab_buff(1);
        *p     = v;
    }
    template <typename T>
    void write_net_num(T v, size_2) noexcept
    {
        auto p = buf_.grab_buff(2);
        x3me::convert::write_htobe16_unsafe(v, p);
    }
    template <typename T>
    void write_net_num(T v, size_4) noexcept
    {
        auto p = buf_.grab_buff(4);
        x3me::convert::write_htobe32_unsafe(v, p);
    }
    template <typename T>
    void write_net_num(T v, size_8) noexcept
    {
        auto p = buf_.grab_buff(8);
        x3me::convert::write_htobe64_unsafe(v, p);
    }
};

} // namespace bin_utils
} // namespace x3me
