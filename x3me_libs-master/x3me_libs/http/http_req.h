#pragma once

#include <string>

#include <boost/http/message.hpp>

namespace x3me
{
namespace net
{

class http_req
{
    friend class http_conn;

    std::string method_;
    std::string path_;
    std::string query_;
    boost::http::message msg_;

private:
    http_req() = default;

public:
    ~http_req() = default;

    http_req(const http_req&) = delete;
    http_req& operator=(const http_req&) = delete;
    http_req(http_req&&) = delete;
    http_req& operator=(http_req&&) = delete;

    const std::string& method() const { return method_; }
    const std::string& path() const { return path_; }
    const std::string& query() const { return query_; }

    // TODO GCC 4.8.2 doesn't support the auto return type yet
    auto headers() -> decltype(msg_.headers()) { return msg_.headers(); }
    auto body() -> decltype(msg_.body()) { return msg_.body(); }
};

} // namespace net
} // namespace x3me
