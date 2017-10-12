namespace cache
{
namespace detail
{

struct fs_version
{
    uint16_t major_;
    uint16_t minor_;

    static constexpr auto create(uint16_t major, uint16_t minor) noexcept
    {
        return fs_version{major, minor};
    }
};
static_assert(std::is_pod<fs_version>::value,
              "Needs to be POD because it maps to raw memory");

inline bool operator==(const fs_version& lhs, const fs_version& rhs) noexcept
{
    return (lhs.major_ == rhs.major_) && (lhs.minor_ == rhs.minor_);
}

inline bool operator!=(const fs_version& lhs, const fs_version& rhs) noexcept
{
    return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os,
                                const fs_version& rhs) noexcept
{
    return os << rhs.major_ << '.' << rhs.minor_;
}

} // namespace detail
} // namespace cache
