#include <cstdlib>
#include <string>

#include "common.h"

#include "../infohash.h"
#include "../peer_id.h"

using namespace x3me::bt_utils;

uint8_t call_srand()
{
    srand(time(nullptr));
    return 0;
}

static const uint8_t c = call_srand();

std::string random_ih()
{
    (void)c; // Suppress warnings for unused variable
    char chars[256];
    for (unsigned i = 0; i < 256; ++i)
    {
        chars[i] = static_cast<char>(i);
    }

    std::string ret;
    ret.reserve(infohash_size);
    for (int i = 0; i < infohash::ssize; ++i)
    {
        ret.push_back(chars[rand() % sizeof(chars)]);
    }
    return ret;
}

std::string random_peer_id()
{
    char chars[256];
    for (unsigned i = 0; i < 256; ++i)
    {
        chars[i] = static_cast<char>(i);
    }

    char valid_chars[] = "abcdefghijklmnopqrstuvwxyz"
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "1234567890";

    std::string ret;
    ret.reserve(peer_id_size);
    int i = 0;
    ret.push_back('-');
    ++i;
    for (; i < 7; ++i)
    {
        ret.push_back(valid_chars[rand() % sizeof(valid_chars)]);
    }
    ret.push_back('-');
    ++i;
    for (; i < peer_id::ssize; ++i)
    {
        ret.push_back(chars[rand() % sizeof(chars)]);
    }
    return ret;
}
