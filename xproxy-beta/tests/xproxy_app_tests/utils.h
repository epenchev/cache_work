#pragma once

constexpr auto operator"" _KB(unsigned long long int v)
{
    return v * 1024;
}

std::string gen_random_data(size_t len);
