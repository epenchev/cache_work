#include "precompiled.h"
#include "tcp_server.h"

tcp_server::tcp_server() : acceptor_(ios_), sock_(ios_)
{
}

bool tcp_server::accept_on(boost::string_view ip, uint16_t po)
{
    assert(!sock_.is_open());
    last_err_.clear();
    try
    {
        using namespace boost::asio::ip;
        const auto addr = address::from_string(ip.data());

        acceptor_.open(tcp::v4());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(tcp::endpoint(addr, po));
        acceptor_.listen(boost::asio::socket_base::max_connections);

        acceptor_.accept(sock_);
    }
    catch (const boost::system::system_error& err)
    {
        std::stringstream ss;
        ss << "Aaccept failed on " << ip << ':' << po << ". "
           << err.code().message();
        last_err_ = ss.str();
        return false;
    }
    return true;
}

bool tcp_server::write(boost::string_view data)
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

boost::string_view tcp_server::read_some(size_t size)
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

bool tcp_server::shutdown_send()
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

bool tcp_server::shutdown_recv()
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

bool tcp_server::close()
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
