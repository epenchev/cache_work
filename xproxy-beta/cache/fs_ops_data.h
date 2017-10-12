#pragma once

namespace cache
{
namespace detail
{

class fs_ops_data // File system operations data
{
    bytes64_t write_pos_;
    // This number counts the write laps on the volume i.e.
    // it increases every time when we reach the end of the disk, wraps
    // and start from the beginning.
    // We need to do different actions if we are on the 0-th lap or not.
    // Theoretically this number can overflow and start back from 0, but
    // we should not see it in practice.
    // Usually a disk gets full for about 24 hours, this means that for
    // an year work we can get 365 wraps. Let's say that we have 1024 wraps
    // for an year. This means that this number will overlap after 2^54 years :)
    uint64_t write_lap_;

public:
    void clean_init(bytes64_t init_write_pos) noexcept // Instead constructor
    {
        write_pos_ = init_write_pos;
        write_lap_ = 0;
    }

    void wrap_write_pos(bytes64_t init_write_pos) noexcept
    {
        write_pos_ = init_write_pos;
        write_lap_ += 1;
    }

    void inc_write_pos(bytes64_t bytes) noexcept { write_pos_ += bytes; }
    void set_write_pos(bytes64_t p) noexcept { write_pos_ = p; }
    void set_write_lap(uint64_t l) noexcept { write_lap_ = l; }

    bytes64_t write_pos() const noexcept { return write_pos_; }
    uint64_t write_lap() const noexcept { return write_lap_; }

    static constexpr auto size() noexcept { return sizeof(fs_ops_data); }
};
static_assert(std::is_trivial<fs_ops_data>::value,
              "Needs to be trivial because it maps to raw memory");

inline std::ostream& operator<<(std::ostream& os,
                                const fs_ops_data& rhs) noexcept
{
    return os << "{write_pos: " << rhs.write_pos()
              << ", write_lap: " << rhs.write_lap() << '}';
}

} // namespace detail
} // namespace cache
