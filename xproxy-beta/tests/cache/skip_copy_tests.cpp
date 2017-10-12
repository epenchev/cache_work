#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/read_buffers.h"
#include "../../cache/skip_copy.h"

using namespace cache::detail;

namespace
{

template <typename... Strings>
auto make_const_buffers(Strings&&... strings) noexcept
{
    return cache::const_buffers{cache::buffer(strings, ::strlen(strings))...};
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(skip_copy_tests)

BOOST_AUTO_TEST_CASE(skip_from_beginning_one_op)
{
    const auto all_len  = 16;
    const auto offs     = 0;
    const auto skip_beg = 4;
    const auto skip_end = 0;
    skip_copy scp(all_len, offs, skip_beg, skip_end);

    read_buffers rbufs;
    rbufs = make_const_buffers("abcdefgh", "ijklmnop");
    uint8_t wbuf[32];

    // Actual testing ////////////////////////////////////////
    const auto res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 4);
    BOOST_CHECK_EQUAL(res.copied_, 12);
    BOOST_CHECK(::memcmp(wbuf, "efghijklmnop", 12) == 0);
    BOOST_CHECK(scp.done());
}

BOOST_AUTO_TEST_CASE(skip_from_beginning_multi_op)
{
    const auto all_len  = 64;
    const auto offs     = 0;
    const auto skip_beg = 4;
    const auto skip_end = 0;
    skip_copy scp(all_len, offs, skip_beg, skip_end);

    uint8_t wbuf[32];
    read_buffers rbufs;

    // The read buffers will end first in this case.
    rbufs    = make_const_buffers("abcdefgh", "ijklmnop");
    auto res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 4);
    BOOST_CHECK_EQUAL(res.copied_, 12);
    BOOST_CHECK(::memcmp(wbuf, "efghijklmnop", 12) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 16);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // This time the write buffer will end up first
    rbufs = make_const_buffers("abcdefgh", "ijklmnop", "qrstuvwx");
    res = scp(rbufs, skip_copy::buffer_t(wbuf, 16));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 16);
    BOOST_CHECK(::memcmp(wbuf, "abcdefghijklmnop", 16) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 32);
    BOOST_CHECK(!rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // Continue reading from the previous rbufs.
    res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 8);
    BOOST_CHECK(::memcmp(wbuf, "qrstuvwx", 8) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 40);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // Let the read_buffers and the write buffer finish at the same time
    rbufs = make_const_buffers("0123456789abcdefghijklmn");
    res = scp(rbufs, skip_copy::buffer_t(wbuf, 24));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 24);
    BOOST_CHECK(::memcmp(wbuf, "0123456789abcdefghijklmn", 24) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 64);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(scp.done());
}

BOOST_AUTO_TEST_CASE(skip_from_end_one_op)
{
    const auto all_len  = 16;
    const auto offs     = 0;
    const auto skip_beg = 0;
    const auto skip_end = 4;
    skip_copy scp(all_len, offs, skip_beg, skip_end);

    read_buffers rbufs;
    rbufs = make_const_buffers("abcdefgh", "ijklmnop");
    uint8_t wbuf[32];

    // Actual testing ////////////////////////////////////////
    const auto res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 4);
    BOOST_CHECK_EQUAL(res.copied_, 12);
    BOOST_CHECK(::memcmp(wbuf, "abcdefghijkl", 12) == 0);
    BOOST_CHECK(scp.done());
}

BOOST_AUTO_TEST_CASE(skip_from_end_multi_op)
{
    const auto all_len  = 64;
    const auto offs     = 0;
    const auto skip_beg = 0;
    const auto skip_end = 4;
    skip_copy scp(all_len, offs, skip_beg, skip_end);

    uint8_t wbuf[32];
    read_buffers rbufs;

    // The read buffers will end first in this case.
    rbufs    = make_const_buffers("abcdefgh", "ijklmnop");
    auto res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 16);
    BOOST_CHECK(::memcmp(wbuf, "abcdefghijklmnop", 16) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 16);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // This time the write buffer will end up first
    rbufs = make_const_buffers("abcdefgh", "ijklmnop", "qrstuvwx");
    res = scp(rbufs, skip_copy::buffer_t(wbuf, 16));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 16);
    BOOST_CHECK(::memcmp(wbuf, "abcdefghijklmnop", 16) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 32);
    BOOST_CHECK(!rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // Continue reading from the previous rbufs.
    res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 8);
    BOOST_CHECK(::memcmp(wbuf, "qrstuvwx", 8) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 40);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // Let the read_buffers and the write buffer finish at the same time
    rbufs = make_const_buffers("0123456789abcdefghijklmn");
    res = scp(rbufs, skip_copy::buffer_t(wbuf, 20));
    BOOST_CHECK_EQUAL(res.skipped_, 4);
    BOOST_CHECK_EQUAL(res.copied_, 20);
    BOOST_CHECK(::memcmp(wbuf, "0123456789abcdefghij", 20) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 64);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(scp.done());
}

BOOST_AUTO_TEST_CASE(skip_from_both_ends_one_op)
{
    const auto all_len  = 16;
    const auto offs     = 0;
    const auto skip_beg = 5;
    const auto skip_end = 4;
    skip_copy scp(all_len, offs, skip_beg, skip_end);

    read_buffers rbufs;
    rbufs = make_const_buffers("abcdefgh", "ijklmnop");
    uint8_t wbuf[32];

    // Actual testing ////////////////////////////////////////
    const auto res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 9);
    BOOST_CHECK_EQUAL(res.copied_, 7);
    BOOST_CHECK(::memcmp(wbuf, "fghijkl", 7) == 0);
    BOOST_CHECK(scp.done());
}

BOOST_AUTO_TEST_CASE(skip_from_both_ends_multi_op)
{
    const auto all_len  = 64;
    const auto offs     = 0;
    const auto skip_beg = 6;
    const auto skip_end = 6;
    skip_copy scp(all_len, offs, skip_beg, skip_end);

    uint8_t wbuf[32];
    read_buffers rbufs;

    // The read buffers will end first in this case.
    rbufs    = make_const_buffers("abcdefgh", "ijklmnop");
    auto res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 6);
    BOOST_CHECK_EQUAL(res.copied_, 10);
    BOOST_CHECK(::memcmp(wbuf, "ghijklmnop", 10) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 16);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // This time the write buffer will end up first
    rbufs = make_const_buffers("abcdefgh", "ijklmnop", "qrstuvwx");
    res = scp(rbufs, skip_copy::buffer_t(wbuf, 16));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 16);
    BOOST_CHECK(::memcmp(wbuf, "abcdefghijklmnop", 16) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 32);
    BOOST_CHECK(!rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // Continue reading from the previous rbufs.
    res = scp(rbufs, skip_copy::buffer_t(wbuf));
    BOOST_CHECK_EQUAL(res.skipped_, 0);
    BOOST_CHECK_EQUAL(res.copied_, 8);
    BOOST_CHECK(::memcmp(wbuf, "qrstuvwx", 8) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 40);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(!scp.done());
    // Let the read_buffers and the write buffer finish at the same time
    rbufs = make_const_buffers("0123456789abcdefghijklmn");
    res = scp(rbufs, skip_copy::buffer_t(wbuf, 18));
    BOOST_CHECK_EQUAL(res.skipped_, 6);
    BOOST_CHECK_EQUAL(res.copied_, 18);
    BOOST_CHECK(::memcmp(wbuf, "0123456789abcdefgh", 18) == 0);
    BOOST_CHECK_EQUAL(scp.curr_offs(), 64);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(scp.done());
}

BOOST_AUTO_TEST_SUITE_END()
