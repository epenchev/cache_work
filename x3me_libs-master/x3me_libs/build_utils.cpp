#include <ostream>
#include <iomanip>

#include "build_utils.h"

// The addresses of these variable are filled with the
// date and the time by the linker
extern char X3ME_BUILD_DATE;
extern char X3ME_BUILD_TIME;
extern char X3ME_GIT_HASH;

namespace x3me
{
namespace utils
{

std::ostream& operator<<(std::ostream& os, const build_datetime& rhs)
{
    auto prev_fill = os.fill('0');
    os << rhs.year_ << '/' << std::setw(2) << rhs.month_ << '/' << std::setw(2)
       << rhs.day_ << ' ' << std::setw(2) << rhs.hours_ << ':' << std::setw(2)
       << rhs.minutes_ << ':' << std::setw(2) << rhs.seconds_;
    os.fill(prev_fill);
    return os;
}

build_datetime get_build_datetime()
{
    auto build_date = (uint64_t)&X3ME_BUILD_DATE;
    auto build_time = (uint64_t)&X3ME_BUILD_TIME;

    build_datetime res;

    res.day_ = build_date % 100;
    build_date /= 100;
    res.month_ = build_date % 100;
    build_date /= 100;
    res.year_ = build_date % 10000;

    res.seconds_ = build_time % 100;
    build_time /= 100;
    res.minutes_ = build_time % 100;
    build_time /= 100;
    res.hours_ = build_time % 100;

    return res;
}

uint32_t get_build_number()
{
    return (uint64_t)&X3ME_BUILD_DATE;
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const git_hash& rhs)
{
    const auto old = os.setf(std::ios_base::hex, std::ios_base::basefield);
    os << rhs.hash_;
    os.setf(old, std::ios_base::basefield); // Restore the base
    return os;
}

git_hash get_git_hash()
{
    return git_hash{(uint64_t)&X3ME_GIT_HASH};
}

} // namespace utils
} // namespace x3me
