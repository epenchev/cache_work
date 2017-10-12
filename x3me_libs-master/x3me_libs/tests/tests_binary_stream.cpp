#include <algorithm>
#include <array>

#include <boost/test/unit_test.hpp>

#include "../binary_stream.h"
#include "../convert.h"

using namespace x3me::bin_utils;
using namespace x3me::convert;

BOOST_AUTO_TEST_SUITE(tests_binary_stream)

BOOST_AUTO_TEST_CASE(static_streambuf_construct)
{
    std::array<int8_t, 16> buf;
    static_streambuf<int8_t> sbuf(buf.data(), buf.size());
    binary_stream<decltype(sbuf)> strm(sbuf);

    BOOST_CHECK(sbuf.data() == buf.data());
    BOOST_CHECK_EQUAL(sbuf.size(), 0);
    BOOST_CHECK_EQUAL(buf.size(), sbuf.capacity());
}

BOOST_AUTO_TEST_CASE(static_streambuf_write_net_order)
{
    uint64_t n64 = 13245324524;
    uint32_t n32 = 1653534;
    int16_t n16  = 3454;
    int8_t n8    = 56;

    std::array<char, 16> expected;
    write_htobe64_unsafe(n64, &expected[0]);
    write_htobe32_unsafe(n32, &expected[8]);
    write_htobe16_unsafe(n16, &expected[12]);
    expected[14] = n8;

    std::array<char, 16> buf;
    static_streambuf<char> sbuf(buf.data(), buf.size());
    binary_stream<decltype(sbuf)> strm(sbuf);

    strm.write_net_order(n64);
    strm.write_net_order(n32);
    strm.write_net_order(n16);
    strm.write_net_order(n8);

    BOOST_CHECK_EQUAL(sbuf.size(), 15);
    BOOST_CHECK(std::equal(buf.cbegin(), buf.cbegin() + 15, expected.cbegin()));
}

BOOST_AUTO_TEST_CASE(static_streambuf_write_host_order)
{
    int64_t n64        = -13245324524;
    int32_t n32        = -1653534;
    uint16_t n16       = 3454;
    uint8_t n8         = 56;
    const int8_t str[] = "abcdefg";

    std::array<int8_t, 32> expected;
    std::memcpy(&expected[0], &n64, 8);
    std::memcpy(&expected[8], &n32, 4);
    std::memcpy(&expected[12], str, 7);
    std::memcpy(&expected[19], &n16, 2);
    expected[21] = n8;

    std::array<int8_t, 32> buf;
    static_streambuf<int8_t> sbuf(buf.data(), buf.size());
    binary_stream<decltype(sbuf)> strm(sbuf);

    strm.write(n64);
    strm.write(n32);
    strm.write(str, 7);
    strm.write(n16);
    strm.write(n8);

    BOOST_CHECK_EQUAL(sbuf.size(), 22);
    BOOST_CHECK(std::equal(buf.cbegin(), buf.cbegin() + 22, expected.cbegin()));
}

BOOST_AUTO_TEST_CASE(static_streambuf_write_mixed_order)
{
    int64_t n64         = -13245324524;
    int32_t n32         = -1653534;
    uint16_t n16        = 3454;
    uint8_t n8          = 56;
    const uint8_t str[] = "abcdefg";

    std::array<uint8_t, 32> expected;
    write_htobe64_unsafe(n64, &expected[0]);
    std::memcpy(&expected[8], &n32, 4);
    write_htobe16_unsafe(n16, &expected[12]);
    std::memcpy(&expected[14], str, 7);
    expected[21] = n8;

    std::array<uint8_t, 32> buf;
    static_streambuf<uint8_t> sbuf(buf.data(), buf.size());
    binary_stream<decltype(sbuf)> strm(sbuf);

    strm.write_net_order(n64);
    strm.write(n32);
    strm.write_net_order(n16);
    strm.write(str, 7);
    strm.write(n8);

    BOOST_CHECK_EQUAL(sbuf.size(), 22);
    BOOST_CHECK(std::equal(buf.cbegin(), buf.cbegin() + 22, expected.cbegin()));
}

BOOST_AUTO_TEST_CASE(dynamic_streambuf_default_construct)
{
    dynamic_streambuf<int8_t> sbuf;
    binary_stream<decltype(sbuf)> strm(sbuf);

    BOOST_CHECK(sbuf.data() == nullptr);
    BOOST_CHECK_EQUAL(sbuf.size(), 0);
    BOOST_CHECK_EQUAL(sbuf.capacity(), 0);
}

BOOST_AUTO_TEST_CASE(dynamic_streambuf_construct_reserve)
{
    dynamic_streambuf<int8_t> sbuf(32);
    binary_stream<decltype(sbuf)> strm(sbuf);

    BOOST_CHECK(sbuf.data() != nullptr);
    BOOST_CHECK_EQUAL(sbuf.size(), 0);
    BOOST_CHECK_EQUAL(sbuf.capacity(), 32);
}

BOOST_AUTO_TEST_CASE(dynamic_streambuf_write_net_order)
{
    uint64_t n64 = 13245324524;
    uint32_t n32 = 1653534;
    int16_t n16  = 3454;
    int8_t n8    = 56;

    std::array<char, 16> expected;
    write_htobe64_unsafe(n64, &expected[0]);
    write_htobe32_unsafe(n32, &expected[8]);
    write_htobe16_unsafe(n16, &expected[12]);
    expected[14] = n8;

    dynamic_streambuf<char> sbuf;
    binary_stream<decltype(sbuf)> strm(sbuf);

    strm.write_net_order(n64);
    strm.write_net_order(n32);
    strm.write_net_order(n16);
    strm.write_net_order(n8);

    auto p    = sbuf.data();
    auto size = sbuf.size();
    auto cap = sbuf.capacity();
    BOOST_CHECK_EQUAL(size, 15);
    BOOST_CHECK(std::equal(p, p + size, expected.cbegin()));

    auto buff = sbuf.release();
    BOOST_CHECK(sbuf.data() == nullptr);
    BOOST_CHECK_EQUAL(sbuf.size(), 0);
    BOOST_CHECK_EQUAL(sbuf.capacity(), 0);

    BOOST_CHECK(buff.buff_.get() == p);
    BOOST_CHECK_EQUAL(buff.size_, size);
    BOOST_CHECK_EQUAL(buff.capacity_, cap);
}

BOOST_AUTO_TEST_CASE(dynamic_streambuf_write_host_order)
{
    int64_t n64        = -13245324524;
    int32_t n32        = -1653534;
    uint16_t n16       = 3454;
    uint8_t n8         = 56;
    const int8_t str[] = "abcdefg";

    std::array<int8_t, 32> expected;
    std::memcpy(&expected[0], &n64, 8);
    std::memcpy(&expected[8], &n32, 4);
    std::memcpy(&expected[12], str, 7);
    std::memcpy(&expected[19], &n16, 2);
    expected[21] = n8;

    dynamic_streambuf<int8_t> sbuf(32);
    binary_stream<decltype(sbuf)> strm(sbuf);

    strm.write(n64);
    strm.write(n32);
    strm.write(str, 7);
    strm.write(n16);
    strm.write(n8);

    auto p    = sbuf.data();
    auto size = sbuf.size();
    auto cap = sbuf.capacity();
    BOOST_CHECK_EQUAL(size, 22);
    BOOST_CHECK(std::equal(p, p + size, expected.cbegin()));

    auto buff = sbuf.release();
    BOOST_CHECK(sbuf.data() == nullptr);
    BOOST_CHECK_EQUAL(sbuf.size(), 0);
    BOOST_CHECK_EQUAL(sbuf.capacity(), 0);

    BOOST_CHECK(buff.buff_.get() == p);
    BOOST_CHECK_EQUAL(buff.size_, size);
    BOOST_CHECK_EQUAL(buff.capacity_, cap);
}

BOOST_AUTO_TEST_CASE(dynamic_streambuf_write_mixed_order)
{
    int64_t n64         = -13245324524;
    int32_t n32         = -1653534;
    uint16_t n16        = 3454;
    uint8_t n8          = 56;
    const uint8_t str[] = "abcdefg";

    std::array<uint8_t, 32> expected;
    write_htobe64_unsafe(n64, &expected[0]);
    std::memcpy(&expected[8], &n32, 4);
    write_htobe16_unsafe(n16, &expected[12]);
    std::memcpy(&expected[14], str, 7);
    expected[21] = n8;

    dynamic_streambuf<uint8_t> sbuf;
    binary_stream<decltype(sbuf)> strm(sbuf);

    strm.write_net_order(n64);
    strm.write(n32);
    strm.write_net_order(n16);
    strm.write(str, 7);
    strm.write(n8);

    auto p    = sbuf.data();
    auto size = sbuf.size();
    auto cap = sbuf.capacity();
    BOOST_CHECK_EQUAL(size, 22);
    BOOST_CHECK(std::equal(p, p + size, expected.cbegin()));

    auto buff = sbuf.release();
    BOOST_CHECK(sbuf.data() == nullptr);
    BOOST_CHECK_EQUAL(sbuf.size(), 0);
    BOOST_CHECK_EQUAL(sbuf.capacity(), 0);

    BOOST_CHECK(buff.buff_.get() == p);
    BOOST_CHECK_EQUAL(buff.size_, size);
    BOOST_CHECK_EQUAL(buff.capacity_, cap);
}

BOOST_AUTO_TEST_SUITE_END()
