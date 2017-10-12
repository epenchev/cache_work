#include <algorithm>
#include <numeric>
#include <string>

#include <boost/test/unit_test.hpp>

#include "common.h"

#include "../stack_buffer.h"

using namespace x3me::mem_utils;

namespace
{

std::string random_str(size_t size)
{
    char chars[256];
    for (unsigned i = 0; i < 256; ++i)
    {
        chars[i] = static_cast<char>(i);
    }

    std::string ret;
    ret.reserve(size);
    for (size_t i = 0; i < size; ++i)
    {
        ret.push_back(chars[rand() % sizeof(chars)]);
    }
    return ret;
}

enum
{
    buff_size_32 = 32
};
using stack_buff_32_t = stack_buffer<char, buff_size_32>;
}

BOOST_AUTO_TEST_SUITE(tests_stack_buffer)

BOOST_AUTO_TEST_CASE(test_construction)
{
    stack_buff_32_t sb;
    BOOST_CHECK(sb.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_assign)
{
    auto s = random_str(32);
    stack_buff_32_t sb;
    sb.assign(s.data(), s.size());
    BOOST_CHECK(sb.size() == s.size());
    BOOST_CHECK((std::equal(s.cbegin(), s.cend(), sb.cbegin()) == true));
}

BOOST_AUTO_TEST_CASE(test_construct_with_data)
{
    auto s = random_str(24);
    stack_buff_32_t sb(s.data(), s.size());
    BOOST_CHECK(sb.size() == s.size());
    BOOST_CHECK((std::equal(s.cbegin(), s.cend(), sb.cbegin()) == true));
}

BOOST_AUTO_TEST_CASE(test_copy_construct)
{
    auto s = random_str(16);
    stack_buff_32_t sb1(s.data(), s.size());
    stack_buff_32_t sb2(sb1);
    BOOST_CHECK(sb1.size() == sb2.size());
    BOOST_CHECK((std::equal(sb1.cbegin(), sb1.cend(), sb2.cbegin()) == true));
}

BOOST_AUTO_TEST_CASE(test_copy_assign)
{
    auto s = random_str(32);
    stack_buff_32_t sb1(s.data(), s.size());
    s[8]  = ~s[8];
    s[16] = ~s[16];
    stack_buff_32_t sb2(s.data(), s.size());
    BOOST_CHECK(sb1.size() == sb2.size());
    BOOST_CHECK((std::equal(sb1.cbegin(), sb1.cend(), sb2.cbegin()) != true));
    sb2 = sb1;
    BOOST_CHECK(sb1.size() == sb2.size());
    BOOST_CHECK((std::equal(sb1.cbegin(), sb1.cend(), sb2.cbegin()) == true));
}

BOOST_AUTO_TEST_CASE(test_append)
{
    auto s = random_str(32);
    stack_buff_32_t sb;
    sb.append(s.data(), 16);
    BOOST_CHECK(sb.size() == 16);
    BOOST_CHECK((std::equal(s.begin(), s.begin() + 16, sb.begin()) == true));
    sb.append(s.data() + 16, 8);
    BOOST_CHECK(sb.size() == 24);
    BOOST_CHECK((std::equal(s.begin(), s.begin() + 24, sb.begin()) == true));
    sb.append(s.data() + 24, 8);
    BOOST_CHECK(sb.size() == s.size());
    BOOST_CHECK((std::equal(s.begin(), s.end(), sb.begin()) == true));
}

BOOST_AUTO_TEST_CASE(test_mem_move)
{
    auto s = random_str(32);
    stack_buff_32_t sb;
    sb.append(s.data(), s.size());
    BOOST_CHECK(sb.size() == s.size());
    sb.mem_move(8);
    BOOST_CHECK(sb.size() == 24);
    BOOST_CHECK((std::equal(sb.cbegin(), sb.cend(), s.begin() + 8) == true));
    sb.mem_move(8);
    BOOST_CHECK(sb.size() == 16);
    BOOST_CHECK((std::equal(sb.cbegin(), sb.cend(), s.begin() + 16) == true));
    sb.mem_move(8);
    BOOST_CHECK(sb.size() == 8);
    BOOST_CHECK((std::equal(sb.cbegin(), sb.cend(), s.begin() + 24) == true));
    sb.mem_move(8);
    BOOST_CHECK(sb.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_const_iterators)
{
    std::string s(24, '0');
    std::iota(s.begin(), s.end(), 'a');
    const stack_buff_32_t sb(s.data(), s.size());
    char v   = 'a';
    bool res = std::all_of(sb.cbegin(), sb.cend(), [&v](char c)
                           {
                               bool res = (v == c);
                               ++v;
                               return res;
                           });
    BOOST_CHECK(res == true);
    v   = 'a';
    res = std::all_of(sb.begin(), sb.end(), [&v](char c)
                      {
                          bool res = (v == c);
                          ++v;
                          return res;
                      });
    BOOST_CHECK(res == true);
}

BOOST_AUTO_TEST_CASE(test_iterators)
{
    std::string s(32, 'a');
    stack_buff_32_t sb(s.data(), s.size());
    char v = 'A';
    for (auto it = sb.begin(); it != sb.end(); ++it)
    {
        *it = v;
        ++v;
    }
    v        = 'A';
    bool res = std::all_of(sb.begin(), sb.end(), [&v](char c)
                           {
                               bool res = (v == c);
                               ++v;
                               return res;
                           });
    BOOST_CHECK(res == true);
}

BOOST_AUTO_TEST_CASE(test_const_reverse_iterators)
{
    std::string s(24, '0');
    std::iota(s.rbegin(), s.rend(), 'a');
    const stack_buff_32_t sb(s.data(), s.size());
    char v   = 'a';
    bool res = std::all_of(sb.crbegin(), sb.crend(), [&v](char c)
                           {
                               bool res = (v == c);
                               ++v;
                               return res;
                           });
    BOOST_CHECK(res == true);
    v   = 'a';
    res = std::all_of(sb.rbegin(), sb.rend(), [&v](char c)
                      {
                          bool res = (v == c);
                          ++v;
                          return res;
                      });
    BOOST_CHECK(res == true);
}

BOOST_AUTO_TEST_CASE(test_reverse_iterators)
{
    std::string s(32, 'a');
    stack_buff_32_t sb(s.data(), s.size());
    char v = 'A';
    for (auto it = sb.rbegin(); it != sb.rend(); ++it)
    {
        *it = v;
        ++v;
    }
    v        = 'A';
    bool res = std::all_of(sb.crbegin(), sb.crend(), [&v](char c)
                           {
                               bool res = (v == c);
                               ++v;
                               return res;
                           });
    BOOST_CHECK(res == true);
}

BOOST_AUTO_TEST_CASE(test_non_const_accessors_and_modifiers)
{
    auto s               = random_str(32);
    size_t expected_size = 0;
    stack_buff_32_t sb1;
    for (const auto& c : s)
    {
        BOOST_CHECK(expected_size == sb1.size());
        sb1.increase_size(1);
        sb1[expected_size] = c;
        ++expected_size;
    }
    BOOST_CHECK(sb1.size() == s.size());
    BOOST_CHECK((std::equal(s.begin(), s.end(), sb1.begin()) == true));

    stack_buff_32_t sb2;
    std::memcpy(sb2.buffer(), s.data(), s.size());
    sb2.set_size(s.size());
    BOOST_CHECK(sb2.size() == s.size());
    BOOST_CHECK((std::equal(s.begin(), s.end(), sb2.begin()) == true));
}

BOOST_AUTO_TEST_CASE(test_const_accessors)
{
    auto s               = random_str(32);
    size_t expected_size = 0;
    stack_buff_32_t sb1;
    for (const auto& c : s)
    {
        BOOST_CHECK(sb1.capacity() - expected_size == sb1.free_size());
        BOOST_CHECK(sb1.buffer() + expected_size == sb1.free_space());
        sb1.increase_size(1);
        *sb1.data_at(expected_size) = c;
        ++expected_size;
    }
    BOOST_CHECK(sb1.full());
    BOOST_CHECK((std::equal(s.begin(), s.end(), sb1.begin()) == true));
}

BOOST_AUTO_TEST_SUITE_END()
