#pragma once

class id_tag // identification tag
{
public:
    using sess_id_t  = uint32_t;
    using trans_id_t = uint16_t;
    using module_t   = uint16_t;

    enum struct module : module_t
    {
        disk,
        http,
        main,
        net,
        plgn,

        max,
    };

private:
    // In order to save 8 bytes we represent the ip and port as
    // separate numbers instead of using tcp_endpoint_v4.
    // The above structure packed size is 6 bytes, but actual size is 8.
    sess_id_t sess_id_   = 0;
    trans_id_t trans_id_ = 0;
    module module_id_    = module::net;
    uint32_t user_ip_    = 0;
    uint32_t serv_ip_    = 0;
    uint16_t user_po_    = 0;
    uint16_t serv_po_    = 0;

public:
    constexpr id_tag() noexcept = default;
    constexpr explicit id_tag(module m) noexcept : module_id_(m) {}

    void set_session_id(sess_id_t v) noexcept { sess_id_ = v; }
    void set_transaction_id(uint16_t v) noexcept { trans_id_ = v; }
    void set_module_id(module v) noexcept { module_id_ = v; }
    void set_user_endpoint(const boost::asio::ip::tcp::endpoint& v) noexcept
    {
        set_ip_port(v, user_ip_, user_po_);
    }
    void set_server_endpoint(const boost::asio::ip::tcp::endpoint& v) noexcept
    {
        set_ip_port(v, serv_ip_, serv_po_);
    }

    auto session_id() const noexcept { return sess_id_; }
    auto transaction_id() const noexcept { return trans_id_; }
    auto module_id() const noexcept { return module_id_; }
    // TODO Rename user to client and server to origin
    tcp_endpoint_v4 user_endpoint() const noexcept
    {
        return tcp_endpoint_v4(user_ip_, user_po_);
    }
    tcp_endpoint_v4 server_endpoint() const noexcept
    {
        return tcp_endpoint_v4(serv_ip_, serv_po_);
    }

    auto user_ip() const noexcept
    {
        return boost::asio::ip::address_v4{user_ip_};
    }
    uint32_t user_ip_num() const noexcept { return user_ip_; }
    uint16_t user_port() const noexcept { return user_po_; }

    auto server_ip() const noexcept
    {
        return boost::asio::ip::address_v4{serv_ip_};
    }
    uint32_t server_ip_num() const noexcept { return serv_ip_; }
    uint16_t server_port() const noexcept { return serv_po_; }

private:
    void set_ip_port(const boost::asio::ip::tcp::endpoint& v,
                     uint32_t& ip,
                     uint16_t& po) noexcept;

    friend bool operator==(const id_tag& lhs, const id_tag& rhs) noexcept;
    friend bool operator!=(const id_tag& lhs, const id_tag& rhs) noexcept;
    friend std::ostream& operator<<(std::ostream&, const id_tag&) noexcept;
};

static_assert(sizeof(id_tag) == 20,
              "Think if you really want to increase the tag size");

// Few empty tags
constexpr id_tag disk_tag{id_tag::module::disk};
constexpr id_tag http_tag{id_tag::module::http};
constexpr id_tag main_tag{id_tag::module::main};
constexpr id_tag net_tag{id_tag::module::net};
constexpr id_tag plgn_tag{id_tag::module::plgn};
