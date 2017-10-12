#pragma once

#include <iterator>
#include <ostream>

#include "encode.h"
#include "detail/bt_utils.h"

namespace x3me
{
namespace bt_utils
{

enum
{
    peer_id_size = 20,
};

class peer_id : public detail::byte_array<char, peer_id_size>
{
public:
    using detail::byte_array<char, peer_id_size>::byte_array;
    using detail::byte_array<char, peer_id_size>::operator=;
};

class peer_id_view : public detail::static_mem_view<char, peer_id_size>
{
public:
    using detail::static_mem_view<char, peer_id_size>::static_mem_view;
};

inline void swap(peer_id& lhs, peer_id& rhs)
{
    lhs.swap(rhs);
}

inline void swap(peer_id_view& lhs, peer_id_view& rhs)
{
    lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& os, const peer_id& pid)
{
    encode::encode_ascii_control_codes(pid.cbegin(), pid.cend(),
                                       std::ostream_iterator<char>(os));
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const peer_id_view& pid)
{
    if (pid)
    {
        encode::encode_ascii_control_codes(pid.cbegin(), pid.cend(),
                                           std::ostream_iterator<char>(os));
    }
    else
    {
        os << "no_peer_id";
    }
    return os;
}

} // namespace bt_utils
} // namespace x3me

namespace std
{

template <>
struct hash<x3me::bt_utils::peer_id>
{
    size_t operator()(const x3me::bt_utils::peer_id& v) const
    {
        return boost::hash_range(v.cbegin(), v.cend());
    }
};

template <>
struct hash<x3me::bt_utils::peer_id_view>
{
    size_t operator()(const x3me::bt_utils::peer_id_view& v) const
    {
        return boost::hash_range(v.cbegin(), v.cend());
    }
};

} // namespace std
