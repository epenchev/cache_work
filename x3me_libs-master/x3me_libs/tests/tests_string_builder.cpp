#include <algorithm>
#include <iomanip>

#include <boost/test/unit_test.hpp>

#include "../string_builder.h"

template <size_t BufferSize>
using string_builder_t = x3me::utils::string_builder<BufferSize>;

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(tests_string_builder)

BOOST_AUTO_TEST_CASE(test_default_construct)
{
    string_builder_t<8> sb;
    BOOST_CHECK(sb.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_streaming)
{
    std::string s  = "1234567";
    std::string ss = s;
    string_builder_t<8> sb;
    for (int i = 0; i < 10; ++i)
    {
        sb << ss;
        BOOST_CHECK_EQUAL(sb.size(), s.size());
        BOOST_CHECK_EQUAL(sb.to_string(), s);
        BOOST_CHECK(std::equal(sb.data(), sb.data() + sb.size(), s.data()));
        s += ss;
    }
}

BOOST_AUTO_TEST_CASE(test_iomanip)
{
    const bool b  = true;
    const float f = 567.8734934;
    std::stringstream ss;
    string_builder_t<8> sb;
    for (int i = 0; i < 50; ++i)
    {
        sb << std::boolalpha << b;
        ss << std::boolalpha << b;

        sb << std::hex << i;
        ss << std::hex << i;

        sb << std::setprecision(3) << f;
        ss << std::setprecision(3) << f;

        BOOST_CHECK_EQUAL(sb.to_string(), ss.str());
    }
}

BOOST_AUTO_TEST_SUITE_END()
