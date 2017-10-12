#pragma once

#include <string>

#include <boost/utility/string_ref.hpp>

namespace std
{
class error_code;
}

////////////////////////////////////////////////////////////////////////////////

namespace x3me
{
namespace http
{

// The functionality is obtained from the Cris Kohloff's urdl library.
// It's just made a little more light weight.
// The offered functionality is very basic and also it doesn't
// unescape any element.
class url
{
    struct elem_pos
    {
        uint32_t beg_ = 0;
        uint32_t len_ = 0;
    };

    std::string full_url_;
    elem_pos protocol_;
    elem_pos user_info_;
    elem_pos host_;
    elem_pos port_;
    elem_pos path_;
    elem_pos query_;
    elem_pos fragment_;
    bool ipv6_host_ = false;

public:
    url() = default;

    url(const char* s) { *this = from_string(s); }
    url(const std::string& s) { *this = from_string(s); }

    boost::string_ref protocol() const { return to_string_ref(protocol_); }

    boost::string_ref user_info() const { return to_string_ref(user_info_); }

    boost::string_ref host() const { return to_string_ref(host_); }

    uint16_t port() const;

    boost::string_ref path() const { return to_string_ref(path_); }

    boost::string_ref query() const { return to_string_ref(query_); }

    boost::string_ref fragment() const { return to_string_ref(fragment_); }

    static url from_string(const char* s, size_t len);
    static url from_string(const char* s, size_t len, std::error_code& ec);

    static url from_string(const std::string& s)
    {
        return from_string(s.c_str(), s.size());
    }
    static url from_string(const std::string& s, std::error_code& ec)
    {
        return from_string(s.c_str(), s.size(), ec);
    }

    friend bool operator==(const url& lhs, const url& rhs)
    {
        return lhs.full_url_ == rhs.full_url_;
    }

    friend bool operator!=(const url& lhs, const url& rhs)
    {
        return lhs.full_url_ != rhs.full_url_;
    }

    friend bool operator<(const url& lhs, const url& rhs)
    {
        return lhs.full_url_ < rhs.full_url_;
    }

private:
    boost::string_ref to_string_ref(const elem_pos& pos) const;
};

} // namespace http
} // namespace x3me
