#pragma once

class tcp_server
{
    std::string last_err_;

    std::vector<char> recv_buff_;

    boost::asio::io_service ios_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket sock_;

public:
    tcp_server();

    bool accept_on(boost::string_view ip, uint16_t po);

    bool write(boost::string_view data);

    boost::string_view read_some(size_t size);

    bool shutdown_send();
    bool shutdown_recv();

    bool close();

    const std::string& last_err() const { return last_err_; }
};
