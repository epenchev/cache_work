#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/write_buffers.h"

using namespace cache::detail;
using cache::mutable_buffers;

namespace
{

constexpr auto operator""_av(const char* s, size_t l) noexcept
{
    using x3me::mem_utils::make_array_view;
    return make_array_view(reinterpret_cast<const uint8_t*>(s), l);
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(write_buffers_tests)

BOOST_AUTO_TEST_CASE(continious_write)
{
    using arr_t = std::array<uint8_t, 4>;
    arr_t b1, b2, b3, b4;

    // clang-format off
    mutable_buffers bufs{
        cache::buffer(b1),
        cache::buffer(b2),
        cache::buffer(b3),
        cache::buffer(b4)
    };
    // clang-format on

    write_buffers wbufs;
    wbufs = std::move(bufs);

    auto written = wbufs.write("abc"_av);
    BOOST_CHECK_EQUAL(written, 3);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 3);
    BOOST_CHECK(!wbufs.all_written());
    // Write 3 more going to the second buffer
    written = wbufs.write("def"_av);
    BOOST_CHECK_EQUAL(written, 3);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 6);
    BOOST_CHECK(!wbufs.all_written());
    // Write 6 more going to the beginning of the last buffer
    written = wbufs.write("ghijkl"_av);
    BOOST_CHECK_EQUAL(written, 6);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 12);
    BOOST_CHECK(!wbufs.all_written());
    // Write last 4 of the last buffer. Try to write more just for a test
    written = wbufs.write("mnopqrst"_av);
    BOOST_CHECK_EQUAL(written, 4);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 16);
    BOOST_CHECK(wbufs.all_written());

    BOOST_CHECK(::memcmp(b1.data(), "abcd", 4) == 0);
    BOOST_CHECK(::memcmp(b2.data(), "efgh", 4) == 0);
    BOOST_CHECK(::memcmp(b3.data(), "ijkl", 4) == 0);
    BOOST_CHECK(::memcmp(b4.data(), "mnop", 4) == 0);
}

BOOST_AUTO_TEST_CASE(move_construction)
{
    using arr_t = std::array<uint8_t, 4>;
    arr_t b1, b2, b3, b4;

    // clang-format off
    mutable_buffers bufs{
        cache::buffer(b1),
        cache::buffer(b2),
        cache::buffer(b3),
        cache::buffer(b4)
    };
    // clang-format on

    write_buffers wbufs;
    wbufs = std::move(bufs);

    auto written = wbufs.write("abc"_av);
    BOOST_CHECK_EQUAL(written, 3);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 3);
    BOOST_CHECK(!wbufs.all_written());
    // Write 3 more going to the second buffer
    written = wbufs.write("def"_av);
    BOOST_CHECK_EQUAL(written, 3);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 6);
    BOOST_CHECK(!wbufs.all_written());

    write_buffers wbufs2(std::move(wbufs));
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 0);
    BOOST_CHECK(wbufs.all_written());
    BOOST_CHECK(wbufs.empty());
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 6);
    BOOST_CHECK(!wbufs2.all_written());
    BOOST_CHECK(!wbufs2.empty());

    // Write 6 more going to the beginning of the last buffer
    written = wbufs2.write("ghijkl"_av);
    BOOST_CHECK_EQUAL(written, 6);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 12);
    BOOST_CHECK(!wbufs2.all_written());
    // Write last 4 of the last buffer. Try to write more just for a test
    written = wbufs2.write("mnopqrst"_av);
    BOOST_CHECK_EQUAL(written, 4);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 16);
    BOOST_CHECK(wbufs2.all_written());

    BOOST_CHECK(::memcmp(b1.data(), "abcd", 4) == 0);
    BOOST_CHECK(::memcmp(b2.data(), "efgh", 4) == 0);
    BOOST_CHECK(::memcmp(b3.data(), "ijkl", 4) == 0);
    BOOST_CHECK(::memcmp(b4.data(), "mnop", 4) == 0);
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    using arr_t = std::array<uint8_t, 4>;
    arr_t b1, b2, b3, b4;

    // clang-format off
    mutable_buffers bufs{
        cache::buffer(b1),
        cache::buffer(b2),
        cache::buffer(b3),
        cache::buffer(b4)
    };
    // clang-format on

    write_buffers wbufs;
    wbufs = std::move(bufs);

    auto written = wbufs.write("abc"_av);
    BOOST_CHECK_EQUAL(written, 3);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 3);
    BOOST_CHECK(!wbufs.all_written());
    // Write 3 more going to the second buffer
    written = wbufs.write("def"_av);
    BOOST_CHECK_EQUAL(written, 3);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 6);
    BOOST_CHECK(!wbufs.all_written());

    write_buffers wbufs2;
    wbufs2 = std::move(wbufs);
    BOOST_CHECK_EQUAL(wbufs.bytes_written(), 0);
    BOOST_CHECK(wbufs.all_written());
    BOOST_CHECK(wbufs.empty());
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 6);
    BOOST_CHECK(!wbufs2.all_written());
    BOOST_CHECK(!wbufs2.empty());

    // Write 6 more going to the beginning of the last buffer
    written = wbufs2.write("ghijkl"_av);
    BOOST_CHECK_EQUAL(written, 6);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 12);
    BOOST_CHECK(!wbufs2.all_written());
    // Write last 4 of the last buffer. Try to write more just for a test
    written = wbufs2.write("mnopqrst"_av);
    BOOST_CHECK_EQUAL(written, 4);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 16);
    BOOST_CHECK(wbufs2.all_written());

    BOOST_CHECK(::memcmp(b1.data(), "abcd", 4) == 0);
    BOOST_CHECK(::memcmp(b2.data(), "efgh", 4) == 0);
    BOOST_CHECK(::memcmp(b3.data(), "ijkl", 4) == 0);
    BOOST_CHECK(::memcmp(b4.data(), "mnop", 4) == 0);
}

BOOST_AUTO_TEST_CASE(swap)
{
    using arr_t = std::array<uint8_t, 4>;
    arr_t b11, b12, b13, b14;
    arr_t b21, b22, b23, b24;

    // clang-format off
    mutable_buffers bufs1{
        cache::buffer(b11),
        cache::buffer(b12),
        cache::buffer(b13),
        cache::buffer(b14)
    };
    mutable_buffers bufs2{
        cache::buffer(b21),
        cache::buffer(b22),
        cache::buffer(b23),
        cache::buffer(b24)
    };
    // clang-format on

    write_buffers wbufs1;
    wbufs1 = std::move(bufs1);
    write_buffers wbufs2;
    wbufs2 = std::move(bufs2);

    auto written = wbufs1.write("abcdefgh"_av);
    BOOST_CHECK_EQUAL(written, 8);
    BOOST_CHECK_EQUAL(wbufs1.bytes_written(), 8);

    written = wbufs2.write("abcdefghij"_av);
    BOOST_CHECK_EQUAL(written, 10);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 10);

    wbufs1.swap(wbufs2);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 8);
    BOOST_CHECK_EQUAL(wbufs1.bytes_written(), 10);

    written = wbufs1.write("abcdefgh"_av);
    BOOST_CHECK_EQUAL(written, 6);
    BOOST_CHECK_EQUAL(wbufs1.bytes_written(), 16);
    BOOST_CHECK(wbufs1.all_written());

    written = wbufs2.write("kmnopqrstuv"_av);
    BOOST_CHECK_EQUAL(written, 8);
    BOOST_CHECK_EQUAL(wbufs2.bytes_written(), 16);
    BOOST_CHECK(wbufs2.all_written());

    BOOST_CHECK(::memcmp(b11.data(), "abcd", 4) == 0);
    BOOST_CHECK(::memcmp(b12.data(), "efgh", 4) == 0);
    BOOST_CHECK(::memcmp(b13.data(), "kmno", 4) == 0);
    BOOST_CHECK(::memcmp(b14.data(), "pqrs", 4) == 0);

    BOOST_CHECK(::memcmp(b21.data(), "abcd", 4) == 0);
    BOOST_CHECK(::memcmp(b22.data(), "efgh", 4) == 0);
    BOOST_CHECK(::memcmp(b23.data(), "ijab", 4) == 0);
    BOOST_CHECK(::memcmp(b24.data(), "cdef", 4) == 0);
}

BOOST_AUTO_TEST_SUITE_END()
