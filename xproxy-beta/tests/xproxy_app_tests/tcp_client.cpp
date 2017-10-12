#include "precompiled.h"
#include "tcp_client.h"

tcp_client::tcp_client() : sock_(ios_)
{
}

bool tcp_client::connect_to(boost::string_view bind_ip, boost::string_view ip,
                            uint16_t po)
{
    try
    {
        using namespace boost::asio::ip;
        const auto bind_addr = address::from_string(bind_ip.data());
        const auto addr      = address::from_string(ip.data());

        sock_.open(tcp::v4());
        sock_.set_option(boost::asio::socket_base::reuse_address(true));
        sock_.bind(tcp::endpoint(bind_addr, 0));
        sock_.connect(tcp::endpoint(addr, po));
    }
    catch (const boost::system::system_error& err)
    {
        std::stringstream ss;
        ss << "Connect failed to " << ip << ':' << po << ". "
           << err.code().message();
        last_err_ = ss.str();
        return false;
    }
    return true;
}

bool tcp_client::write(boost::string_view data)
{
    last_err_.clear();
    boost::system::error_code err;
    boost::asio::write(sock_, boost::asio::buffer(data.data(), data.size()),
                       err);
    if (err)
    {
        std::stringstream ss;
        ss << "Write failed. " << err.message();
        last_err_ = ss.str();
        return false;
    }
    return true;
}

boost::string_view tcp_client::read_some(size_t size)
{
    last_err_.clear();
    recv_buff_.resize(size);
    std::string v;
    boost::system::error_code err;
    const auto len = sock_.read_some(boost::asio::buffer(recv_buff_), err);
    if (err && (err != boost::asio::error::eof))
    {
        std::stringstream ss;
        ss << "Read_some failed. " << err.message();
        last_err_ = ss.str();
    }
    return boost::string_view{recv_buff_.data(), len};
}

bool tcp_client::shutdown_send()
{
    last_err_.clear();
    boost::system::error_code err;
    sock_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, err);
    if (err)
    {
        std::stringstream ss;
        ss << "Shutdown_send failed. " << err.message();
        last_err_ = ss.str();
        return false;
    }
    return true;
}

bool tcp_client::shutdown_recv()
{
    last_err_.clear();
    boost::system::error_code err;
    sock_.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, err);
    if (err)
    {
        std::stringstream ss;
        ss << "Shutdown_recv failed. " << err.message();
        last_err_ = ss.str();
        return false;
    }
    return true;
}

bool tcp_client::close()
{
    last_err_.clear();
    boost::system::error_code err;
    sock_.close(err);
    if (err)
    {
        std::stringstream ss;
        ss << "Close failed. " << err.message();
        last_err_ = ss.str();
        return false;
    }
    return true;
}
