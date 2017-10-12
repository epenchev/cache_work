#pragma once

namespace cache
{
namespace detail
{

// TODO Remove if not needed
class mark_as_read_res
{
public:
    enum res : uint32_t
    {
        ok,
        key_not_found,
        rng_not_found,
        limit_reached, // readers limit reached
        not_set,
    };

private:
    res res_;
    uint32_t cnt_ranges_;

public:
    explicit mark_as_read_res(res r, uint32_t cnt = 0) noexcept
        : res_(r),
          cnt_ranges_(cnt)
    {
    }
    mark_as_read_res() noexcept : mark_as_read_res(mark_as_read_res::not_set) {}

    uint32_t cnt_ranges() const noexcept { return cnt_ranges_; }
    res result() const noexcept { return res_; }
    explicit operator bool() const noexcept { return res_ == ok; }
};

inline std::ostream& operator<<(std::ostream& os,
                                const mark_as_read_res& rhs) noexcept
{
    switch (rhs.result())
    {
    case mark_as_read_res::ok:
        os << "{Res: Ok, Cnt_rngs: ";
        break;
    case mark_as_read_res::key_not_found:
        os << "{Res: Key not found, Cnt_rngs: ";
        break;
    case mark_as_read_res::rng_not_found:
        os << "{Res: Range not found, Cnt_rngs: ";
        break;
    case mark_as_read_res::limit_reached:
        os << "{Res: Readers limit reached, Cnt_rngs: ";
        break;
    case mark_as_read_res::not_set:
        os << "{Res: Not set, Cnt_rngs: ";
        break;
    default:
        X3ME_ASSERT(false, "Missing switch case");
        break;
    }
    os << rhs.cnt_ranges() << '}';
    return os;
}

} // namespace detail
} // namespace cache
