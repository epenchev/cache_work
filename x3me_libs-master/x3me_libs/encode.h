#pragma once

#include <cassert>
#include <iterator>
#include <type_traits>

namespace x3me
{
namespace encode
{
namespace detail
{

inline auto hex_char(uint8_t pos)
{
    return "0123456789ABCDEF"[pos];
}

template <typename T, typename OutIt>
void encode_ascii_cc(const T& v, OutIt& out)
{
    using val_t = typename std::make_unsigned<T>::type;
    static_assert(std::is_same<val_t, uint8_t>::value,
                  "The encoding of ASCII control codes works only with "
                  "singed/unsigned chars");
    // The printable ASCII characters are between space (0x20) and ~ (0x7E)
    if (((v >= 0x20) && (v <= 0x7E)) || (v == '\t') || (v == '\v') ||
        (v == '\r') || (v == '\n'))
    {
        *out++ = v;
    }
    else // Write the hex code of the symbol
    {
        typename std::make_unsigned<T>::type uv = v;

        *out++ = '\\';
        *out++ = 'x';
        *out++ = hex_char(uv >> 4);
        *out++ = hex_char(uv & 0x0F);
    }
}

template <typename T, typename OutIt>
void url_encode_char(const T& v, OutIt& out)
{
    using val_t = typename std::make_unsigned<T>::type;
    static_assert(std::is_same<val_t, uint8_t>::value,
                  "The url_encode works only with singed/unsigned chars");
    if ((v == '-') || (v == '.') || (v == '_') || (v == '~') ||
        ((v >= '0') && (v <= '9')) || ((v >= 'A') && (v <= 'Z')) ||
        ((v >= 'a') && (v <= 'z')))
    {
        *out++ = v;
    }
    else
    {
        typename std::make_unsigned<T>::type uv = v;

        *out++ = '%';
        *out++ = hex_char(uv >> 4);
        *out++ = hex_char(uv & 0x0F);
    }
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////

template <typename InIt, typename OutIt>
OutIt encode_ascii_control_codes(InIt beg, InIt end, OutIt out)
{
    for (; beg != end; ++beg)
        detail::encode_ascii_cc(*beg, out);
    return out;
}

// We need SFINAE to help the overload resolution to
// pickup the correct function
// between the one with beg-end iterators and this one
template <typename T, typename Size, typename OutIt>
typename std::enable_if<std::is_integral<Size>::value, OutIt>::type
encode_ascii_control_codes(const T* p, Size size, OutIt out)
{
    assert(size >= 0);
    return encode_ascii_control_codes(p, p + size, out);
}

template <typename Range, typename OutIt>
OutIt encode_ascii_control_codes(const Range& r, OutIt out)
{
    return encode_ascii_control_codes(std::begin(r), std::end(r), out);
}

////////////////////////////////////////////////////////////////////////////////

template <typename InIt, typename OutIt>
OutIt url_encode(InIt beg, InIt end, OutIt out)
{
    for (; beg != end; ++beg)
        detail::url_encode_char(*beg, out);
    return out;
}

// We need SFINAE to help the overload resolution to
// pickup the correct function
// between the one with beg-end iterators and this one
template <typename T, typename Size, typename OutIt>
typename std::enable_if<std::is_integral<Size>::value, OutIt>::type
url_encode(const T* p, Size size, OutIt out)
{
    assert(size >= 0);
    return url_encode(p, p + size, out);
}

template <typename Range, typename OutIt>
OutIt url_encode(const Range& r, OutIt out)
{
    return url_encode(std::begin(r), std::end(r), out);
}

} // namespace encode
} // namespace x3me
