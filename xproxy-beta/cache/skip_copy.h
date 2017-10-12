#pragma once

namespace cache
{
namespace detail
{

// A class facilitating copying data from which we want to skip
// some bytes from the beginning and from the end.
class skip_copy
{
    bytes64_t curr_offs_;
    const bytes64_t all_len_;
    const bytes64_t data_beg_;
    const bytes64_t data_end_;

public:
    skip_copy(bytes64_t all_len,
              bytes64_t cur_off,
              bytes64_t data_beg,
              bytes64_t data_end) noexcept : curr_offs_(cur_off),
                                             all_len_(all_len),
                                             data_beg_(data_beg),
                                             data_end_(data_end)
    {
        X3ME_ASSERT(cur_off <= all_len, "The offset must be inside the data");
        X3ME_ASSERT((bytes64_t(data_beg) + data_end) <= all_len,
                    "The skipped part can't be bigger than the data");
    }

    struct bytes
    {
        bytes64_t skipped_ = 0;
        bytes64_t copied_  = 0;
    };
    using buffer_t = x3me::mem_utils::array_view<uint8_t>;
    template <typename Rdr>
    bytes operator()(Rdr& from, buffer_t to) noexcept;

    bool done() const noexcept { return curr_offs_ == all_len_; }

    auto all_data_len() const noexcept { return all_len_; }
    auto curr_offs() const noexcept { return curr_offs_; }
    auto data_beg_offs() const noexcept { return data_beg_; }
    auto data_end_offs() const noexcept { return data_end_; }

    friend std::ostream& operator<<(std::ostream& os,
                                    const skip_copy::bytes& rhs) noexcept;
    friend std::ostream& operator<<(std::ostream& os,
                                    const skip_copy& rhs) noexcept;
};

////////////////////////////////////////////////////////////////////////////////

template <typename Rdr>
skip_copy::bytes skip_copy::operator()(Rdr& from, buffer_t to) noexcept
{
    bytes ret;

    const bytes64_t data_beg = data_beg_;
    const bytes64_t data_end = all_len_ - data_end_;

    if (curr_offs_ < data_beg)
    {
        // Skip_reading from 'from', if there is nothing to read, is safe.
        const auto skip = from.skip_read(data_beg - curr_offs_);
        curr_offs_ += skip;
        ret.skipped_ += skip;
    }

    if (x3me::math::in_range(curr_offs_, data_beg, data_end))
    {
        const auto to_read = std::min(data_end - curr_offs_, to.size());
        // Reading from 'from', if there is nothing to be read, is safe.
        const auto read_bytes = from.read(buffer_t{to.data(), to_read});
        curr_offs_ += read_bytes;
        X3ME_ASSERT(curr_offs_ <= data_end, "Wrong function logic");
        ret.copied_ += read_bytes;
    }

    if (x3me::math::in_range(curr_offs_, data_end, all_len_))
    {
        const auto skip = from.skip_read(all_len_ - curr_offs_);
        curr_offs_ += skip;
        ret.skipped_ += skip;
    }

    X3ME_ASSERT(curr_offs_ <= all_len_, "Wrong function logic");

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

inline std::ostream& operator<<(std::ostream& os,
                                const skip_copy::bytes& rhs) noexcept
{
    return os << "{skipped: " << rhs.skipped_ << ", copied: " << rhs.copied_
              << '}';
}

inline std::ostream& operator<<(std::ostream& os, const skip_copy& rhs) noexcept
{
    // clang-format off
    return os << "{dall: " << rhs.all_len_ 
              << ", dbeg: " << rhs.data_beg_
              << ", dend: " << (rhs.all_len_ - rhs.data_end_)
              << ", coff: " << rhs.curr_offs_ << '}';
    // clang-format on
}

} // namespace detail
} // namespace cache
