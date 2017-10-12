#pragma once

#include <stdint.h>

#include <iosfwd>

namespace x3me
{
namespace utils
{

// Don't want to use the system tm structure for 2 reasons
// 1. Its fields has slightly different semantics
// 2. It's size is bigger
struct build_datetime
{
    uint16_t year_;
    uint16_t month_;
    uint16_t day_;
    uint16_t hours_;
    uint16_t minutes_;
    uint16_t seconds_;
};

std::ostream& operator<<(std::ostream& os, const build_datetime& rhs);

build_datetime get_build_datetime();

uint32_t get_build_number(); // Returns six digit build number - yymmdd

struct git_hash
{
    uint64_t hash_;
};

std::ostream& operator<<(std::ostream& os, const git_hash& rhs);

git_hash get_git_hash();

} // namespace utils
} // namespace x3me
