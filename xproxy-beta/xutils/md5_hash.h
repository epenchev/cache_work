#pragma once

namespace xutils
{

class md5_hash
{
    friend class md5_hasher;
    uint8_t data_[MD5_DIGEST_LENGTH];

public:
    enum : uint32_t
    {
        ssize = MD5_DIGEST_LENGTH
    };

public:
    md5_hash() noexcept = default;
    md5_hash(const md5_hash&) noexcept = default;
    md5_hash& operator=(const md5_hash&) noexcept = default;
    ~md5_hash() noexcept = default;

    static md5_hash zero() noexcept
    {
        md5_hash res;
        ::memset(res.data_, 0, ssize);
        return res;
    }

    md5_hash(const void* data, size_t size) noexcept { set(data, size); }

    void set(const void* data, size_t size) noexcept
    {
        ::MD5(static_cast<const uint8_t*>(data), size, data_);
    }

    const uint8_t* data() const noexcept { return data_; }
    size_t size() const noexcept { return ssize; }

    const uint8_t* begin() const noexcept { return data_; }
    const uint8_t* end() const noexcept { return data_ + ssize; }

    uint8_t* buff_unsafe() noexcept { return data_; }
};

inline bool operator==(const md5_hash& lhs, const md5_hash& rhs) noexcept
{
    return (memcmp(lhs.data(), rhs.data(), rhs.size()) == 0);
}
inline bool operator<(const md5_hash& lhs, const md5_hash& rhs) noexcept
{
    return (memcmp(lhs.data(), rhs.data(), rhs.size()) < 0);
}
inline bool operator!=(const md5_hash& lhs, const md5_hash& rhs) noexcept
{
    return !(lhs == rhs);
}
inline bool operator>(const md5_hash& lhs, const md5_hash& rhs) noexcept
{
    return (rhs < lhs);
}
inline bool operator<=(const md5_hash& lhs, const md5_hash& rhs) noexcept
{
    return !(rhs < lhs);
}
inline bool operator>=(const md5_hash& lhs, const md5_hash& rhs) noexcept
{
    return !(lhs < rhs);
}
inline std::ostream& operator<<(std::ostream& os, const md5_hash& rhs)
{
    boost::algorithm::hex(rhs.begin(), rhs.end(),
                          std::ostream_iterator<char>(os));
    return os;
}

////////////////////////////////////////////////////////////////////////////////

class md5_hasher
{
    MD5_CTX ctx_;

public:
    md5_hasher() noexcept
    {
        const auto res = ::MD5_Init(&ctx_);
        assert(res);
    }
    ~md5_hasher() noexcept = default;

    md5_hasher(const md5_hasher&) = delete;
    md5_hasher& operator=(const md5_hasher&) = delete;
    md5_hasher(md5_hasher&&) = delete;
    md5_hasher& operator=(md5_hasher&&) = delete;

    void update(const void* data, size_t len) noexcept
    {
        const auto res = ::MD5_Update(&ctx_, data, len);
        assert(res);
    }

    md5_hash final_hash() noexcept
    {
        md5_hash res;
        const auto r = ::MD5_Final(res.data_, &ctx_);
        assert(r);
        return res;
    }
};

} // namespace xutils
