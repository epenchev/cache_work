#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/read_buffers.h"

using namespace cache::detail;
using cache::const_buffer;
using cache::const_buffers;
using x3me::mem_utils::make_array_view;

BOOST_AUTO_TEST_SUITE(read_buffers_tests)

BOOST_AUTO_TEST_CASE(continious_read)
{
    const uint8_t s1[] = "abcd";
    const uint8_t s2[] = "efgh";
    const uint8_t s3[] = "ijkl";
    const uint8_t s4[] = "monp";

    // clang-format off
    const_buffers bufs{
        const_buffer(s1, sizeof(s1) - 1),
        const_buffer(s2, sizeof(s2) - 1),
        const_buffer(s3, sizeof(s3) - 1)
    };
    // clang-format on
    bufs.emplace_back(s4, sizeof(s4) - 1);

    read_buffers rbufs;
    rbufs = std::move(bufs);

    uint8_t r[8];
    // Read first 3 bytes
    auto read_bytes = rbufs.read(read_buffers::buffer_t(r, 3));
    BOOST_REQUIRE_EQUAL(read_bytes, 3);
    BOOST_CHECK(::memcmp(r, "abc", 3) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 3);
    BOOST_CHECK(!rbufs.all_read());
    // Read next 3 bytes going from the first iov to the second one
    read_bytes = rbufs.read(read_buffers::buffer_t(r, 3));
    BOOST_REQUIRE_EQUAL(read_bytes, 3);
    BOOST_CHECK(::memcmp(r, "def", 3) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 6);
    BOOST_CHECK(!rbufs.all_read());
    // Read next 6 bytes going from the second to the third iov, finishing it.
    read_bytes = rbufs.read(read_buffers::buffer_t(r, 6));
    BOOST_REQUIRE_EQUAL(read_bytes, 6);
    BOOST_CHECK(::memcmp(r, "ghijkl", 6) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 12);
    BOOST_CHECK(!rbufs.all_read());
    // Read the last 4 bytes for the last iov, finishing it.
    read_bytes = rbufs.read(read_buffers::buffer_t(r, 4));
    BOOST_REQUIRE_EQUAL(read_bytes, 4);
    BOOST_CHECK(::memcmp(r, "monp", 4) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 16);
    BOOST_CHECK(rbufs.all_read());
}

BOOST_AUTO_TEST_CASE(skip_read)
{
    const uint8_t s1[] = "abcd";
    const uint8_t s2[] = "efgh";
    const uint8_t s3[] = "ijkl";
    const uint8_t s4[] = "monp";

    // clang-format off
    const_buffers bufs{
        const_buffer(s1, sizeof(s1) - 1),
        const_buffer(s2, sizeof(s2) - 1),
        const_buffer(s3, sizeof(s3) - 1)
    };
    // clang-format on
    bufs.emplace_back(s4, sizeof(s4) - 1);

    read_buffers rbufs;
    rbufs = std::move(bufs);

    uint8_t r[8];
    // Skip first 2 bytes
    auto read_bytes = rbufs.skip_read(2);
    BOOST_REQUIRE_EQUAL(read_bytes, 2);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 2);
    BOOST_CHECK(!rbufs.all_read());
    // Read next 3 bytes going from the first iov to the second one
    read_bytes = rbufs.read(read_buffers::buffer_t(r, 3));
    BOOST_REQUIRE_EQUAL(read_bytes, 3);
    BOOST_CHECK(::memcmp(r, "cde", 3) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 5);
    BOOST_CHECK(!rbufs.all_read());
    // Go to the third buffer skipping 4 bytes
    read_bytes = rbufs.skip_read(4);
    BOOST_REQUIRE_EQUAL(read_bytes, 4);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 9);
    BOOST_CHECK(!rbufs.all_read());
    // Read 2 bytes, remaining in the third buffer
    read_bytes = rbufs.read(read_buffers::buffer_t(r, 2));
    BOOST_REQUIRE_EQUAL(read_bytes, 2);
    BOOST_CHECK(::memcmp(r, "jk", 2) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 11);
    BOOST_CHECK(!rbufs.all_read());
    // Skip 1 byte and thus go to the last buffer
    read_bytes = rbufs.skip_read(1);
    BOOST_REQUIRE_EQUAL(read_bytes, 1);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 12);
    BOOST_CHECK(!rbufs.all_read());
    // Skip 2 bytes, remaining in the last buffer
    read_bytes = rbufs.skip_read(2);
    BOOST_REQUIRE_EQUAL(read_bytes, 2);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 14);
    BOOST_CHECK(!rbufs.all_read());
    // Skip last two bytes finishing the reading
    read_bytes = rbufs.skip_read(42);
    BOOST_REQUIRE_EQUAL(read_bytes, 2);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 16);
    BOOST_CHECK(rbufs.all_read());
}

BOOST_AUTO_TEST_CASE(move_construction)
{
    const uint8_t s1[] = "abcd";
    const uint8_t s2[] = "efgh";
    const uint8_t s3[] = "ijkl";
    const uint8_t s4[] = "monp";

    // clang-format off
    const_buffers bufs{
        const_buffer(s1, sizeof(s1) - 1),
        const_buffer(s2, sizeof(s2) - 1),
        const_buffer(s3, sizeof(s3) - 1)
    };
    // clang-format on
    bufs.emplace_back(s4, sizeof(s4) - 1);

    read_buffers rbufs;
    rbufs = std::move(bufs);

    // Skip first 7 bytes
    auto read_bytes = rbufs.skip_read(7);
    BOOST_REQUIRE_EQUAL(read_bytes, 7);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 7);
    BOOST_CHECK(!rbufs.all_read());

    read_buffers rbufs2(std::move(rbufs));

    BOOST_CHECK(rbufs.empty());
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 0);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(rbufs.empty());
    BOOST_CHECK_EQUAL(rbufs2.bytes_read(), 7);
    BOOST_CHECK(!rbufs2.all_read());

    uint8_t r[16];
    read_bytes = rbufs2.read(read_buffers::buffer_t(r, 16));
    BOOST_REQUIRE_EQUAL(read_bytes, 9);
    BOOST_CHECK(::memcmp(r, "hijklmonp", 9) == 0);
    BOOST_CHECK_EQUAL(rbufs2.bytes_read(), 16);
    BOOST_CHECK(rbufs2.all_read());
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    const uint8_t s1[] = "abcd";
    const uint8_t s2[] = "efgh";
    const uint8_t s3[] = "ijkl";
    const uint8_t s4[] = "monp";

    // clang-format off
    const_buffers bufs{
        const_buffer(s1, sizeof(s1) - 1),
        const_buffer(s2, sizeof(s2) - 1),
        const_buffer(s3, sizeof(s3) - 1)
    };
    // clang-format on
    bufs.emplace_back(s4, sizeof(s4) - 1);

    read_buffers rbufs;
    rbufs = std::move(bufs);

    // Skip first 7 bytes
    auto read_bytes = rbufs.skip_read(15);
    BOOST_REQUIRE_EQUAL(read_bytes, 15);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 15);
    BOOST_CHECK(!rbufs.all_read());

    read_buffers rbufs2(std::move(rbufs));

    BOOST_CHECK(rbufs.empty());
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 0);
    BOOST_CHECK(rbufs.all_read());
    BOOST_CHECK(rbufs.empty());
    BOOST_CHECK_EQUAL(rbufs2.bytes_read(), 15);
    BOOST_CHECK(!rbufs2.all_read());

    uint8_t r[16];
    read_bytes = rbufs2.read(read_buffers::buffer_t(r, 16));
    BOOST_REQUIRE_EQUAL(read_bytes, 1);
    BOOST_CHECK(::memcmp(r, "p", 1) == 0);
    BOOST_CHECK_EQUAL(rbufs2.bytes_read(), 16);
    BOOST_CHECK(rbufs2.all_read());
}

BOOST_AUTO_TEST_CASE(swap)
{
    const uint8_t s1[] = "abcd";
    const uint8_t s2[] = "efgh";
    const uint8_t s3[] = "ijkl";
    const uint8_t s4[] = "monp";

    // clang-format off
    const_buffers bufs{
        const_buffer(s1, sizeof(s1) - 1),
        const_buffer(s2, sizeof(s2) - 1),
        const_buffer(s3, sizeof(s3) - 1)
    };
    // clang-format on
    bufs.emplace_back(s4, sizeof(s4) - 1);

    // clang-format off
    const_buffers bufs2{
        const_buffer(s4, sizeof(s4) - 1),
        const_buffer(s3, sizeof(s3) - 1),
        const_buffer(s2, sizeof(s2) - 1),
        const_buffer(s1, sizeof(s1) - 1)
    };
    // clang-format on

    read_buffers rbufs;
    rbufs = std::move(bufs);
    read_buffers rbufs2;
    rbufs2 = std::move(bufs2);

    auto read_bytes = rbufs.skip_read(7);
    BOOST_REQUIRE_EQUAL(read_bytes, 7);

    read_bytes = rbufs2.skip_read(9);
    BOOST_REQUIRE_EQUAL(read_bytes, 9);

    rbufs.swap(rbufs2);

    uint8_t r[16];
    read_bytes = rbufs.read(read_buffers::buffer_t(r, 16));
    BOOST_REQUIRE_EQUAL(read_bytes, 7);
    BOOST_CHECK(::memcmp(r, "fghabcd", 7) == 0);
    BOOST_CHECK_EQUAL(rbufs.bytes_read(), 16);
    BOOST_CHECK(rbufs.all_read());

    read_bytes = rbufs2.read(read_buffers::buffer_t(r, 7));
    BOOST_REQUIRE_EQUAL(read_bytes, 7);
    BOOST_CHECK(::memcmp(r, "hijklmo", 7) == 0);
    BOOST_CHECK_EQUAL(rbufs2.bytes_read(), 14);
    BOOST_CHECK(!rbufs2.all_read());
}

BOOST_AUTO_TEST_SUITE_END()
