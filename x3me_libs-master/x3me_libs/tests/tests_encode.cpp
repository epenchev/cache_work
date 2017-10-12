#include <limits>
#include <typeinfo>
#include <vector>

#include <boost/algorithm/hex.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/utility/string_ref.hpp>

#include <curl/curl.h>

#include "../encode.h"
#include "../scope_guard.h"

using namespace x3me::encode;

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
std::string encode_ascii_cc(const std::vector<T>& vr)
{
    std::string s;
    for (const auto& v : vr)
    {
        if (((v >= 0x20) && (v <= 0x7E)) || (v == '\t') || (v == '\v') ||
            (v == '\r') || (v == '\n'))
        {
            s.push_back(v);
        }
        else // Write the hex code of the symbol
        {
            s.push_back('\\');
            s.push_back('x');
            T arr[1] = {v};
            boost::algorithm::hex(std::begin(arr), std::end(arr),
                                  std::back_inserter(s));
        }
    }
    return s;
}

template <typename T>
void test_with_type()
{
    auto curl = curl_easy_init();
    X3ME_SCOPE_EXIT { curl_easy_cleanup(curl); };

    auto v       = generate_input<T>();
    auto s       = encode_ascii_cc(v);
    auto curl_us = curl_easy_escape(
        curl, reinterpret_cast<const char*>(v.data()), v.size());
    X3ME_SCOPE_EXIT { curl_free(curl_us); };
    boost::string_ref us(curl_us);

    {
        std::string r;
        encode_ascii_control_codes(v.cbegin(), v.cend(), std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, s);
        r.clear();
        url_encode(v.cbegin(), v.cend(), std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, us);
    }
    {
        std::string r;
        encode_ascii_control_codes(v.data(), v.size(), std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, s);
        r.clear();
        url_encode(v.data(), v.size(), std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, us);
    }
    {
        std::string r;
        encode_ascii_control_codes(v, std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, s);
        BOOST_TEST_MESSAGE("Type: " << typeid(T).name() << "\nEnc: " << r);
        r.clear();
        url_encode(v, std::back_inserter(r));
        BOOST_CHECK_EQUAL(r, us);
        BOOST_TEST_MESSAGE("Type: " << typeid(T).name() << "\nEnc: " << r);
    }
}
}

BOOST_AUTO_TEST_SUITE(tests_encode)

BOOST_AUTO_TEST_CASE(test_with_char)
{
    test_with_type<char>();
}

BOOST_AUTO_TEST_CASE(test_with_schar)
{
    test_with_type<signed char>();
}

BOOST_AUTO_TEST_CASE(test_with_uchar)
{
    test_with_type<unsigned char>();
}

BOOST_AUTO_TEST_CASE(test_with_int8)
{
    test_with_type<int8_t>();
}

BOOST_AUTO_TEST_CASE(test_with_uint8)
{
    test_with_type<uint8_t>();
}

BOOST_AUTO_TEST_SUITE_END()
