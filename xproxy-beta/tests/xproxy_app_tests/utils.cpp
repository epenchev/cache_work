#include "precompiled.h"
#include "utils.h"

std::string gen_random_data(size_t len)
{
    const char chrs[] = "0123456789"
                        "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    std::mt19937 rg{std::random_device{}()};
    std::uniform_int_distribution<> pick(0, sizeof(chrs) - 2);

    std::string s;
    s.resize(len);

    for (size_t i = 0; i < len; ++i)
        s[i] = chrs[pick(rg)];

    return s;
}
