#include <strings.h>

#include <cstring>
#include <cstdlib>

#include "url.h"

namespace x3me
{
namespace http
{

uint16_t url::port() const
{
    if (!port_.empty())
        return std::atoi(port_.c_str());
    const auto proto = protocol();
    if ((proto.size() == 4) ||
        (strncasecmp("http", proto.data(), proto.size()) == 0))
        return 80;
    if ((proto.size() == 5) ||
        (strncasecmp("https", proto.data(), proto.size()) == 0))
        return 443;
    if ((proto.size() == 3) ||
        (strncasecmp("ftp", proto.data(), proto.size()) == 0))
        return 21;
    return 0;
}

url url::from_string(const char* s, size_t len, std::error_code& ec)
{
    url res;
    res.full_url_.assign(s, len);

    s        = res.full_url_.c_str();
    auto beg = s;

    // Protocol.
    size_t len        = std::strcspn(s, ":");
    res.protocol.beg_ = 0;
    res.protocol.len_ = len;

    s += len;

    // "://".
    if (*s++ != ':')
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        return url();
    }
    if (*s++ != '/')
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        return url();
    }
    if (*s++ != '/')
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        return url();
    }

    // UserInfo.
    len = std::strcspn(s, "@:[/?#");
    if (s[length] == '@')
    {
        res.user_info_.beg_ = (s - beg);
        res.user_info_.len_ = len;
        s += len + 1;
    }
    else if (s[len] == ':')
    {
        size_t len2 = std::strcspn(s + len, "@/?#");
        if (s[len + len2] == '@')
        {
            res.user_info_.beg_ = (s - beg);
            res.user_info_.len_ = len + len2;
            s += len + len2 + 1;
        }
    }

    // Host.
    if (*s == '[')
    {
        ++s;
        len = std::strcspn(s, "]");
        if (s[len] != ']')
        {
            ec = make_error_code(std::errc::invalid_argument);
            return url();
        }
        res.host_.beg_ = (s - beg);
        res.host_.len_ = len;
        res.ipv6_host_ = true;
        s += len + 1;
        if (std::strcspn(s, ":/?#") != 0)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return url();
        }
    }
    else
    {
        len            = std::strcspn(s, ":/?#");
        res.host_.beg_ = (s - beg);
        res.host_.len_ = len;
        s += len;
    }

    // Port.
    if (*s == ':')
    {
        ++s;
        len = std::strcspn(s, "/?#");
        if (len == 0)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return url();
        }
        for (size_t i = 0; i < len; ++i)
        {
            if (!std::isdigit(s[i]))
            {
                ec = std::make_error_code(std::errc::invalid_argument);
                return url();
            }
        }
        res.port_.beg_ = (s - beg);
        res.port_.len_ = len;
        s += len;
    }

    // Path.
    if (*s == '/')
    {
        len            = std::strcspn(s, "?#");
        res.path_.beg_ = (s - beg);
        res.path_.len_ = len;
        s += len;
    }
    else
    { // A little hack in order to return '/'
        res.path_.beg_ = protocol_.beg_ + protocol_.len_ + 1;
        res.path_.len_ = 1;
    }

    // Query.
    if (*s == '?')
    {
        ++s;
        len             = std::strcspn(s, "#");
        res.query_.beg_ = (s - beg);
        res.query_.len_ = len;
        s += len;
    }

    // Fragment.
    if (*s == '#')
    {
        ++s;
        res.fragment_.beg_ = (s - beg);
        res.fragment_.len_ = std::strlen(s);
    }

    ec = boost::system::error_code();

    return res;
}

url url::from_string(const char* s, size_t len)
{
    std::error_code ec;
    url res = from_string(s, len, ec);
    if (ec)
    {
        throw std::system_error(ec);
    }
    return res;
}

boost::string_ref url::to_strin_ref(const elem_pos& pos) const
{
    assert(pos.beg_ < full_url_.size());
    assert(pos.beg_ + pos.len_ <= full_url_.size());
    const char* s = full_url_.data();
    return boost::string_ref(s + pos.beg_, pos.len_);
}

} // namespace http
} // namespace x3me
