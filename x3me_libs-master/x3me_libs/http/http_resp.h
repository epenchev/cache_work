#pragma once

#include <memory>

#include <boost/http/message.hpp>

namespace x3me
{
namespace net
{

class http_resp
{
    // This is going to be copied inside the async handlers
    std::shared_ptr<boost::http::message> msg_;

public:
    http_resp() : msg_(std::make_shared<boost::http::message>()) {}

    // TODO GCC 4.8.2 doesn't support the auto return type yet
    auto headers() -> decltype(msg_->headers()) { return msg_->headers(); }
    auto body() -> decltype(msg_->body()) { return msg_->body(); }

private:
    friend class http_conn;
    const boost::http::message& get() const { return *msg_; }
};

} // namespace net
} // namespace x3me
