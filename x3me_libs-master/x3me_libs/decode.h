#pragma once

#include <cctype>
#include <type_traits>

#include "utils.h"

namespace x3me
{
namespace decode
{
namespace detail
{

enum : uint8_t
{
    invld_res = 0xFF
};

template <typename T>
uint8_t decode_hex_char_part(const T& v)
{
    using val_t = typename std::make_unsigned<T>::type;
    static_assert(std::is_same<val_t, uint8_t>::value,
                  "The decode_hex_char works only with singed/unsigned chars");
    switch (v)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return v - '0';
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
        return v - 'a' + 10;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
        return v - 'A' + 10;
    }
    return invld_res;
}

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

template <typename OutIt>
class result
{
    OutIt out_it_;
    size_t err_pos_;
    bool ok_;

public:
    result(OutIt out_it, size_t err_pos, bool ok)
        : out_it_(out_it), err_pos_(err_pos), ok_(ok)
    {
    }
    explicit operator bool() const { return ok_; }
    size_t err_pos() const { return err_pos_; }
    OutIt iterator() const { return out_it_; }
};

////////////////////////////////////////////////////////////////////////////////

template <typename InIt, typename OutIt>
result<OutIt> url_decode(InIt beg, InIt end, OutIt out)
{
    for (size_t pos = 0; beg != end; ++beg, ++pos)
    {
        switch (*beg)
        {
        case '%':
        {
            uint8_t part1, part2;
            if ((++beg == end) || ((part1 = detail::decode_hex_char_part(
                                        *beg)) == detail::invld_res))
            {
                return result<OutIt>{out, pos, false};
            }
            if ((++beg == end) || ((part2 = detail::decode_hex_char_part(
                                        *beg)) == detail::invld_res))
            {
                return result<OutIt>{out, pos, false};
            }
            pos += 2;
            *out++ = ((part1 << 4) | part2);
            break;
        }
        case '-':
        case '_':
        case '.':
        case '!':
        case '~':
        case '*':
        case '\'':
        case '(':
        case ')':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
        case '$':
        case ',':
        case '/':
        case ';':
        {
            const auto c = *beg;
            *out++       = c;
            break;
        }
        default:
        {
            const auto c = *beg;
            if (!std::isalnum(c))
                return result<OutIt>{out, pos, false};
            *out++ = c;
            break;
        }
        }
    }
    return result<OutIt>{out, size_t(-1), true};
}

// We need SFINAE to help the overload resolution to
// pickup the correct function
// between the one with beg-end iterators and this one
template <typename T, typename Size, typename OutIt>
typename std::enable_if<std::is_integral<Size>::value, result<OutIt>>::type
url_decode(const T* p, Size size, OutIt out)
{
    assert(size >= 0);
    return url_decode(p, p + size, out);
}

template <typename Range, typename OutIt>
result<OutIt> url_decode(const Range& r, OutIt out)
{
    return url_decode(std::begin(r), std::end(r), out);
}

} // namespace decode
} // namespace x3me
