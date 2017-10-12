#pragma once

class tcp_endpoint_v4
{
    boost::asio::ip::address_v4 ip_;
    uint16_t po_ = 0;

public:
    tcp_endpoint_v4() noexcept {}
    tcp_endpoint_v4(const boost::asio::ip::address_v4& ip, uint16_t po) noexcept
        : ip_(ip),
          po_(po)
    {
    }
    /// Expects the ip in host byte order
    tcp_endpoint_v4(uint32_t ip, uint16_t po) noexcept : ip_(ip), po_(po) {}

    const auto& address() const noexcept { return ip_; }
    uint16_t port() const noexcept { return po_; }

    void set_address(const boost::asio::ip::address_v4& ip) noexcept
    {
        ip_ = ip;
    }
    void set_port(uint16_t po) noexcept { po_ = po; }

    friend bool operator<(const tcp_endpoint_v4& lhs,
                          const tcp_endpoint_v4& rhs) noexcept
    {
        // Lexicographical compare as in the std::pair or std::tuple
        return (lhs.ip_ < rhs.ip_) ||
               (!(rhs.ip_ < lhs.ip_) && (lhs.po_ < rhs.po_));
    }

    friend bool operator==(const tcp_endpoint_v4& lhs,
                           const tcp_endpoint_v4& rhs) noexcept
    {
        return (lhs.ip_ == rhs.ip_) && (lhs.po_ == rhs.po_);
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const tcp_endpoint_v4& rhs) noexcept
    {
        os << rhs.ip_ << ':' << rhs.po_;
        return os;
    }
};

////////////////////////////////////////////////////////////////////////////////

namespace bsys       = boost::system;
namespace asio_error = boost::asio::error;

namespace x3me_sockopt
{
using transparent_mode =
    boost::asio::detail::socket_option::boolean<IPPROTO_IP, IP_TRANSPARENT>;
using type_of_service =
    boost::asio::detail::socket_option::integer<IPPROTO_IP, IP_TOS>;
using tcp_keep_idle =
    boost::asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPIDLE>;
using tcp_keep_cnt =
    boost::asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPCNT>;
using tcp_keep_intvl =
    boost::asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPINTVL>;
}

template <typename T>
using optional_t = std::experimental::optional<T>;
template <typename T, typename E>
using expected_t = boost::expected<T, E>;

using io_service_t    = boost::asio::io_service;
using tcp_socket_t    = boost::asio::ip::tcp::socket;
using tcp_acceptor_t  = boost::asio::ip::tcp::acceptor;
using std_timer_t     = boost::asio::steady_timer;
using asio_shutdown_t = boost::asio::socket_base::shutdown_type;
using std_clock_t     = std::chrono::steady_clock;
using signal_set_t    = boost::asio::signal_set;
using ip_addr_t       = boost::asio::ip::address;
using ip_addr4_t      = boost::asio::ip::address_v4;
using tcp_endpoint_t  = boost::asio::ip::tcp::endpoint;
using tcp_endpoint4_t = tcp_endpoint_v4;
using err_code_t      = boost::system::error_code;
using string_view_t   = boost::string_ref;
using uuid_t          = boost::uuids::uuid;
using boost_string_t  = boost::container::string;

using false_sharing_pad_t = char[64];

using net_thread_id_t = uint16_t;

template <size_t Size>
using stack_string_t = x3me::str_utils::stack_string<Size>;
using const_string_t = x3me::str_utils::const_string;

template <typename T>
using owner_ptr_t = T*;
template <typename T>
using non_owner_ptr_t = T*;

////////////////////////////////////////////////////////////////////////////////

using bytes8_t  = uint8_t;
using bytes16_t = uint16_t;
using bytes32_t = uint32_t;
using bytes64_t = uint64_t;
