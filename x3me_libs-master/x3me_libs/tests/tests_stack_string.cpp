#include <cstring>
#include <sstream>
#include <string>

#include <boost/test/unit_test.hpp>

#include "../stack_string.h"

using namespace x3me::str_utils;

BOOST_AUTO_TEST_SUITE(tests_stack_string)

BOOST_AUTO_TEST_CASE(empty_string_len)
{
    stack_string<24> ss;
    BOOST_CHECK_EQUAL(ss.capacity(), 23);
    BOOST_CHECK_EQUAL(std::strlen(ss.data()), 0);
    BOOST_CHECK_EQUAL(ss.size(), 0);
}

BOOST_AUTO_TEST_CASE(no_overflow_string)
{
    const char* str  = "0123456";
    const char* str2 = "321";
    {
        stack_string<8> ss(str);
        BOOST_CHECK_EQUAL(ss.size(), std::strlen(str));
        BOOST_CHECK(std::strcmp(ss.c_str(), str) == 0);
        ss.assign(str2);
        BOOST_CHECK_EQUAL(ss.size(), std::strlen(str2));
        BOOST_CHECK(std::strcmp(ss.c_str(), str2) == 0);
    }
    {
        stack_string<8> ss(str, std::strlen(str));
        BOOST_CHECK_EQUAL(ss.size(), std::strlen(str));
        BOOST_CHECK(std::strcmp(ss.c_str(), str) == 0);
        ss.assign(str2, std::strlen(str2));
        BOOST_CHECK_EQUAL(ss.size(), std::strlen(str2));
        BOOST_CHECK(std::strcmp(ss.c_str(), str2) == 0);
    }
}

BOOST_AUTO_TEST_CASE(overflow_string)
{
    constexpr int ss_size = 8;
    const char* str       = "0123456789";
    const char* str2      = "9876543210";
    {
        stack_string<ss_size> ss(str);
        BOOST_CHECK_EQUAL(ss.size(), ss_size - 1);
        BOOST_CHECK(std::strcmp(ss.c_str(), str) < 0);
        ss.assign(str2);
        BOOST_CHECK_EQUAL(ss.size(), ss_size - 1);
        BOOST_CHECK(std::strcmp(ss.c_str(), str2) < 0);
    }
    {
        stack_string<ss_size> ss(str, std::strlen(str));
        BOOST_CHECK_EQUAL(ss.size(), ss_size - 1);
        BOOST_CHECK(std::strcmp(ss.c_str(), str) < 0);
        ss.assign(str2, std::strlen(str2));
        BOOST_CHECK_EQUAL(ss.size(), ss_size - 1);
        BOOST_CHECK(std::strcmp(ss.c_str(), str2) < 0);
    }
}

BOOST_AUTO_TEST_CASE(compare_strings)
{
    const std::string str  = "0123456";
    const std::string str2 = "321";
    const std::string str3 = "0123456";

    stack_string<8> ss(str.data(), str.size());
    stack_string<8> ss2(str2.data(), str2.size());
    stack_string<8> ss3(str3.data(), str3.size());

    BOOST_CHECK_EQUAL((str == str2), (ss == ss2));
    BOOST_CHECK_EQUAL((str == str3), (ss == ss3));
    BOOST_CHECK_EQUAL((str != str2), (ss != ss2));
    BOOST_CHECK_EQUAL((str != str3), (ss != ss3));
    BOOST_CHECK_EQUAL((str < str2), (ss < ss2));
    BOOST_CHECK_EQUAL((str < str3), (ss < ss3));
    BOOST_CHECK_EQUAL((str2 < str), (ss2 < ss));
    BOOST_CHECK_EQUAL((str3 < str), (ss3 < ss));
}

BOOST_AUTO_TEST_CASE(stream_string)
{
    constexpr int ss_size = 8;
    const char* str = "012345";
    stack_string<ss_size> ss(str);
    std::stringstream sstrm;
    sstrm << ss;
    auto ss_str = sstrm.str();
    BOOST_CHECK_EQUAL(ss.size(), ss_str.size());
    BOOST_CHECK(std::strcmp(ss.c_str(), ss_str.c_str()) == 0);
}

BOOST_AUTO_TEST_SUITE_END()
