#pragma once

namespace xlog
{

using level_type = uint16_t;
enum class level : level_type
{
    off,
    fatal,
    error,
    warn,
    info,
    debug,
    trace,

    num_levels, // Don't use it as a level
};

/// Returns null terminated string corresponding to the logging level
constexpr inline const char* level_str(level lvl) noexcept
{
    static_assert(static_cast<level_type>(level::num_levels) == 7,
                  "The below string streaming needs to be adjusted");
    constexpr uint16_t lbl_len = 6;
    return &"off\0\0\0"
            "fatal\0"
            "error\0"
            "warn\0\0"
            "info\0\0"
            "debug\0"
            "trace\0"[(static_cast<level_type>(lvl) * lbl_len)];
}

inline std::ostream& operator<<(std::ostream& os, level lvl) noexcept
{
    os << level_str(lvl);
    return os;
}

constexpr inline level_type to_number(level l) noexcept
{
    return static_cast<level_type>(l);
}

// The function converts numeric level to enum class 'level'.
// It truncates the number to the max possible level if it's greater.
constexpr inline level to_level(level_type l) noexcept
{
    return (l < to_number(level::num_levels)) ? static_cast<level>(l)
                                              : level::trace;
}

////////////////////////////////////////////////////////////////////////////////

class target_id
{
    uint16_t i_;

public:
    constexpr explicit target_id(uint16_t i) noexcept : i_(i) {}
    constexpr uint16_t value() const noexcept { return i_; }
};

class channel_id
{
    uint16_t i_;

public:
    constexpr explicit channel_id(uint16_t i) noexcept : i_(i) {}
    constexpr uint16_t value() const noexcept { return i_; }
};

constexpr target_id invalid_target_id((uint16_t)-1);
constexpr channel_id invalid_channel_id((uint16_t)-1);

} // namespace xlog
