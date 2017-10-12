#pragma once

#include <iterator>
#include <ostream>

#include <boost/algorithm/hex.hpp>

#include "detail/bt_utils.h"

namespace x3me
{
namespace bt_utils
{

enum
{
    infohash_size = 20,
};

class infohash : public detail::byte_array<char, infohash_size>
{
public:
    using detail::byte_array<char, infohash_size>::byte_array;
    using detail::byte_array<char, infohash_size>::operator=;
};

class infohash_view : public detail::static_mem_view<char, infohash_size>
{
public:
    using detail::static_mem_view<char, infohash_size>::static_mem_view;
};

inline void swap(infohash& lhs, infohash& rhs)
{
    lhs.swap(rhs);
}

inline void swap(infohash_view& lhs, infohash_view& rhs)
{
    lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& os, const infohash& ih)
{
    boost::algorithm::hex(ih.cbegin(), ih.cend(),
                          std::ostream_iterator<char>(os));
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const infohash_view& ih)
{
    if (ih)
    {
        boost::algorithm::hex(ih.cbegin(), ih.cend(),
                              std::ostream_iterator<char>(os));
    }
    else
    {
        os << "no_infohash";
    }
    return os;
}

inline bool operator==(const infohash& lhs, const infohash_view& rhs)
{
    return (std::memcmp(lhs.data(), rhs.data(), infohash_size) == 0);
}

inline bool operator==(const infohash_view& lhs, const infohash& rhs)
{
    return (std::memcmp(lhs.data(), rhs.data(), infohash_size) == 0);
}

inline bool operator!=(const infohash& lhs, const infohash_view& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const infohash_view& lhs, const infohash& rhs)
{
    return !(lhs == rhs);
}

} // namespace bt_utils
} // namespace x3me

namespace std
{

template <>
struct hash<x3me::bt_utils::infohash>
{
    size_t operator()(const x3me::bt_utils::infohash& v) const
    {
        return boost::hash_range(v.cbegin(), v.cend());
    }
};

template <>
struct hash<x3me::bt_utils::infohash_view>
{
    size_t operator()(const x3me::bt_utils::infohash_view& v) const
    {
        return boost::hash_range(v.cbegin(), v.cend());
    }
};

} // namespace std
