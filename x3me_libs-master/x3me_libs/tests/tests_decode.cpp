#include <iostream>

#include <limits>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/utility/string_ref.hpp>

#include <curl/curl.h>

#include "../decode.h"
#include "../scope_guard.h"

using namespace x3me::decode;

namespace
{

template <typename T>
std::vector<T> generate_input()
{
    static_assert(sizeof(T) == 1, "");
    std::vector<T> v;
    auto i = std::numeric_limits<T>::min();
    for (; i != std::numeric_limits<T>::max(); ++i)
    {
        v.push_back(i);
    }
    v.push_back(i); // push the max value
    return v;
}

template <typename T>
void test_with_type()
{
    auto curl = curl_easy_init();
    X3ME_SCOPE_EXIT { curl_easy_cleanup(curl); };

    auto v = generate_input<T>();
    boost::string_ref raw(reinterpret_cast<const char*>(v.data()), v.size());
    auto curl_us = curl_easy_escape(curl, raw.data(), raw.size());
    X3ME_SCOPE_EXIT { curl_free(curl_us); };
    boost::string_ref us(curl_us);

    {
        std::string r;
        auto res = url_decode(us.cbegin(), us.cend(), std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, raw);
        BOOST_CHECK(res);
        BOOST_CHECK_EQUAL(res.err_pos(), size_t(-1));
    }
    {
        std::string r;
        auto res = url_decode(us.data(), us.size(), std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, raw);
        BOOST_CHECK(res);
        BOOST_CHECK_EQUAL(res.err_pos(), size_t(-1));
    }
    {
        std::string r;
        auto res = url_decode(us, std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, raw);
        BOOST_CHECK(res);
        BOOST_CHECK_EQUAL(res.err_pos(), size_t(-1));
    }
}
}

BOOST_AUTO_TEST_SUITE(tests_encode)

BOOST_AUTO_TEST_CASE(test_with_char_valid)
{
    test_with_type<char>();
}

BOOST_AUTO_TEST_CASE(test_with_schar_valid)
{
    test_with_type<signed char>();
}

BOOST_AUTO_TEST_CASE(test_with_uchar_valid)
{
    test_with_type<unsigned char>();
}

BOOST_AUTO_TEST_CASE(test_with_int8_valid)
{
    test_with_type<int8_t>();
}

BOOST_AUTO_TEST_CASE(test_with_uint8_valid)
{
    test_with_type<uint8_t>();
}

BOOST_AUTO_TEST_CASE(test_invalid_char)
{
    char input[] = {'a', 'b', 'c', 'd', 20, 'e', 'f', 'g'};
    std::string out;
    auto res = url_decode(input, std::back_inserter(out));
    BOOST_CHECK(!res);
    BOOST_CHECK_EQUAL(res.err_pos(), 4);
}

BOOST_AUTO_TEST_CASE(test_invalid_char_after_percent)
{
    char input[] = {'a', 'b', 'c', 'd', '%', 'f', 'g'};
    std::string out;
    auto res = url_decode(input, std::back_inserter(out));
    BOOST_CHECK(!res);
    BOOST_CHECK_EQUAL(res.err_pos(), 4);
    char input2[] = {'a', 'b', 'c', 'd', '%', 'g', 'f'};
    out.clear();
    res = url_decode(input2, std::back_inserter(out));
    BOOST_CHECK(!res);
    BOOST_CHECK_EQUAL(res.err_pos(), 4);
}

BOOST_AUTO_TEST_CASE(test_not_enough_chars_after_percent)
{
    char input[] = {'a', 'b', 'c', 'd', '%', 'f'};
    std::string out;
    auto res = url_decode(input, std::back_inserter(out));
    BOOST_CHECK(!res);
    BOOST_CHECK_EQUAL(res.err_pos(), 4);
    char input2[] = {'a', 'b', 'c', 'd', '%'};
    out.clear();
    res = url_decode(input2, std::back_inserter(out));
    BOOST_CHECK(!res);
    BOOST_CHECK_EQUAL(res.err_pos(), 4);
}

BOOST_AUTO_TEST_CASE(test_correct)
{
    boost::string_ref input("http%3A%2F%2F10.240.254.99%3A7879%2Fannounce");
    std::string out;
    auto res = url_decode(input, std::back_inserter(out));
    BOOST_CHECK(res);
    BOOST_CHECK(out == "http://10.240.254.99:7879/announce");
}

BOOST_AUTO_TEST_CASE(test_correct_in_place)
{

    boost::string_ref input("http%3A%2F%2F10.240.254.99%3A7879%2Fannounce");
    std::string inout(input.begin(), input.end());
    auto res = url_decode(inout, inout.begin());
    inout.erase(res.iterator(), inout.end());
    BOOST_CHECK(res);
    BOOST_CHECK_EQUAL(inout, "http://10.240.254.99:7879/announce");
}

BOOST_AUTO_TEST_SUITE_END()
