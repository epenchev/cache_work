#include "precompiled.h"
#include "id_tag.h"

namespace
{

void print_ip(std::ostream& os, uint32_t ip) noexcept
{
    char buf[16];
    auto p = static_cast<const uint8_t*>(static_cast<const void*>(&ip));
    snprintf(buf, sizeof(buf), "%hhu.%hhu.%hhu.%hhu", p[3], p[2], p[1], p[0]);
    os << buf;
}

void print_ipp(std::ostream& os, uint32_t ip, uint16_t po) noexcept
{
    char buf[22];
    auto p = static_cast<const uint8_t*>(static_cast<const void*>(&ip));
    snprintf(buf, sizeof(buf), "%hhu.%hhu.%hhu.%hhu:%hu", p[3], p[2], p[1],
             p[0], po);
    os << buf;
}

constexpr inline const char* module_str(id_tag::module m) noexcept
{
    using module_t = std::underlying_type_t<id_tag::module>;
    static_assert(static_cast<module_t>(id_tag::module::max) == 5,
                  "The below string streaming needs to be adjusted");
    constexpr uint16_t lbl_len = 5;
    return &"disk\0"
            "http\0"
            "main\0"
            "net\0\0"
            "plgn\0"[(static_cast<module_t>(m) * lbl_len)];
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

void id_tag::set_ip_port(const boost::asio::ip::tcp::endpoint& v,
                         uint32_t& ip,
                         uint16_t& po) noexcept
{
    const auto addr = v.address();
    assert(addr.is_v4());
    ip = addr.to_v4().to_ulong();
    po = v.port();
}

bool operator==(const id_tag& lhs, const id_tag& rhs) noexcept
{
    constexpr auto member_size =
        sizeof(id_tag::sess_id_) + sizeof(id_tag::trans_id_) +
        sizeof(id_tag::module_id_) + sizeof(id_tag::user_ip_) +
        sizeof(id_tag::serv_ip_) + sizeof(id_tag::user_po_) +
        sizeof(id_tag::serv_po_);
    static_assert(member_size == sizeof(id_tag),
                  "We use memcmp. There must be no holes");
    return (memcmp(&lhs, &rhs, sizeof(id_tag)) == 0);
}

bool operator!=(const id_tag& lhs, const id_tag& rhs) noexcept
{
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const id_tag& rhs) noexcept
{
    // Tag printng - #mod #sid #tid #uip:upo #sip

    using std::ios_base;
    os.setf(ios_base::left, ios_base::adjustfield);

    // Print the module_id.
    os << '#';
    os.width(4);
    os << module_str(rhs.module_id_);

    // Print session id. 32 bit number i.e. max 10 digits
    os << " #";
    os.width(10);
    os << rhs.sess_id_;

    // Print transaction id. It's expected to have less than 1000 transactions
    // in a single session almost all of the time i.e. 3 digits.
    os << " #";
    os.width(3);
    os << rhs.trans_id_;

    // Print user ip and port. The max possible length is 15 chars for the
    // dotted ip plus ':' plus 5 chars for the port number.
    os << " #";
    os.width(21);
    print_ipp(os, rhs.user_ip_, rhs.user_po_);

    // Print server ip (port is always 80). The max possible length is 15 chars.
    os << " #";
    os.width(15);
    print_ip(os, rhs.serv_ip_);

    // We don't restore the alignment here as we should.
    // We save one call using the knowledge that the id_tag is printed
    // currently only for logging purposes and the current logging logic
    // doesn't need restoring of the alignment.

    return os;
}
