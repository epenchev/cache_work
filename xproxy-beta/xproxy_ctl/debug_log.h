#pragma once

class debug_log
{
    static bool enabled_;

public:
    static void enable(bool v) noexcept { enabled_ = v; }
    static bool enabled() noexcept { return enabled_; }
};

#define LOG_DEBG(...)                                                          \
    if (debug_log::enabled())                                                  \
    std::cout << __VA_ARGS__ << '\n'
#define LOG_INFO(...) std::cout << __VA_ARGS__ << '\n'
#define LOG_ERRR(...) std::cerr << __VA_ARGS__ << '\n'
