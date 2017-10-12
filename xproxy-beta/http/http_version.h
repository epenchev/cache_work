#pragma once

namespace http
{

struct http_version
{
    uint16_t major_ = 0;
    uint16_t minor_ = 0;

    constexpr http_version(uint16_t major, uint16_t minor) noexcept
        : major_(major),
          minor_(minor)
    {
    }
};

inline bool operator==(const http_version& lhs,
                       const http_version& rhs) noexcept
{
    return (lhs.major_ == rhs.major_) && (lhs.minor_ == rhs.minor_);
}

inline bool operator!=(const http_version& lhs,
                       const http_version& rhs) noexcept
{
    return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os,
                                const http_version& rhs) noexcept
{
    return os << rhs.major_ << '.' << rhs.minor_;
}

} // namespace http
