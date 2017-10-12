#pragma once

#include <iterator>
#include <ostream>

#include <boost/algorithm/hex.hpp>

#include "encode.h"

namespace x3me
{
namespace print_utils
{

template <typename It>
struct text_printer
{
    It beg_, end_;
    text_printer(It beg, It end) noexcept : beg_(beg), end_(end) {}
    friend std::ostream& operator<<(std::ostream& os,
                                    const text_printer& rhs) noexcept
    {
        using val_t = typename std::iterator_traits<It>::value_type;
        encode::encode_ascii_control_codes(rhs.beg_, rhs.end_,
                                           std::ostream_iterator<val_t>(os));
        return os;
    }
};

template <typename It>
auto print_text(It beg, It end) noexcept
{
    return text_printer<It>(beg, end);
}

// We need SFINAE to help the overload resolution to
// pickup the correct function
// between the one with beg-end iterators and this one
template <typename T, typename Size>
typename std::enable_if<std::is_integral<Size>::value,
                        text_printer<const T*>>::type
print_text(const T* p, Size size) noexcept
{
    return text_printer<const T*>(p, p + size);
}

template <typename Range>
auto print_text(const Range& r) noexcept
{
    auto it = std::begin(r);
    return text_printer<decltype(it)>(it, std::end(r));
}

////////////////////////////////////////////////////////////////////////////////

template <typename It>
struct lim_text_printer
{
    It beg_, end_;
    bool lim_;
    lim_text_printer(It beg, It end, size_t lim) noexcept : beg_(beg)
    {
        const auto len = std::distance(beg, end);
        assert(len >= 0);
        end_ = std::next(beg_, std::min<size_t>(len, lim));
        lim_ = lim < static_cast<size_t>(len);
    }
    friend std::ostream& operator<<(std::ostream& os,
                                    const lim_text_printer& rhs) noexcept
    {
        using val_t = typename std::iterator_traits<It>::value_type;
        encode::encode_ascii_control_codes(rhs.beg_, rhs.end_,
                                           std::ostream_iterator<val_t>(os));
        if (rhs.lim_)
            os.write(" ...", 4); // Mark that the text has been limited
        return os;
    }
};

template <typename It>
auto print_lim_text(It beg, It end, size_t lim) noexcept
{
    return lim_text_printer<It>(beg, end, lim);
}

// We need SFINAE to help the overload resolution to
// pickup the correct function
// between the one with beg-end iterators and this one
template <typename T, typename Size>
typename std::enable_if<std::is_integral<Size>::value,
                        lim_text_printer<const T*>>::type
print_lim_text(const T* p, Size size, size_t lim) noexcept
{
    return lim_text_printer<const T*>(p, p + size, lim);
}

template <typename Range>
auto print_lim_text(const Range& r, size_t lim) noexcept
{
    auto it = std::begin(r);
    return lim_text_printer<decltype(it)>(it, std::end(r), lim);
}

////////////////////////////////////////////////////////////////////////////////

template <typename It>
struct hex_printer
{
    It beg_, end_;
    hex_printer(It beg, It end) noexcept : beg_(beg), end_(end) {}
    friend std::ostream& operator<<(std::ostream& os,
                                    const hex_printer& rhs) noexcept
    {
        using val_t = typename std::iterator_traits<It>::value_type;
        boost::algorithm::hex(rhs.beg_, rhs.end_,
                              std::ostream_iterator<val_t>(os));
        return os;
    }
};

template <typename It>
auto print_hex(It beg, It end) noexcept
{
    return hex_printer<It>(beg, end);
}

// We need SFINAE to help the overload resolution to
// pickup the correct function
// between the one with beg-end iterators and this one
template <typename T, typename Size>
typename std::enable_if<std::is_integral<Size>::value,
                        hex_printer<const T*>>::type
print_hex(const T* p, Size size) noexcept
{
    return hex_printer<const T*>(p, p + size);
}

template <typename Range>
auto print_hex(const Range& r) noexcept
{
    auto it = std::begin(r);
    return hex_printer<decltype(it)>(it, std::end(r));
}

} // namespace print_utils
} // namespace x3me
