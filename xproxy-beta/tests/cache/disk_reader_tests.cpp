#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/cache_common.h"
#include "../../cache/disk_reader.h"
#include "../../cache/volume_fd.h"

using namespace cache::detail;

namespace
{

constexpr auto bsize         = store_block_size;
constexpr bytes64_t dsize    = 4 * bsize;
constexpr bytes64_t beg_offs = store_block_size;
constexpr bytes64_t end_offs = store_block_size + dsize;
constexpr bytes64_t fsize    = store_block_size + dsize + store_block_size;
const std::string fname      = "/tmp/disk_reader_tests";

struct fixture
{

    fixture()
    {
        volume_fd fd;

        {
            std::ofstream f{fname}; // Touch
        }

        err_code_t err;
        fd.open(fname.c_str(), err);
        BOOST_REQUIRE_MESSAGE(!err, "Unable to open '" + fname + "'. " +
                                        err.message());

        fd.truncate(fsize, err);
        BOOST_REQUIRE_MESSAGE(!err, "Unable to truncate '" + fname + "'. " +
                                        err.message());

        char c   = 'a';
        auto buf = alloc_page_aligned(bsize);
        for (auto i = 0U; i < dsize; i += bsize, ++c)
        {
            ::memset(buf.get(), c, bsize);

            fd.write(buf.get(), bsize, beg_offs + i, err);
            BOOST_REQUIRE_MESSAGE(!err, "Unable to write to '" + fname + "'. " +
                                            err.message());
        }
    }
};

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(disk_reader_tests, fixture)

BOOST_AUTO_TEST_CASE(continuous_read)
{
    char c = 'a';
    disk_reader rdr(boost::container::string{fname.c_str()}, beg_offs,
                    end_offs);

    static_assert(4_KB == dsize / 4, "");
    std::array<uint8_t, 4_KB> buf, tmp;

    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), c, tmp.size());
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size() - 4));
    ::memset(tmp.data(), c + 1, tmp.size() - 4);
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    // Now if we try to read next chunk, it should provoke disk read
    // the small buffer should be exhausted.
    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), c + 1, 4);
    ::memset(tmp.data() + 4, c + 2, tmp.size() - 4);
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    // This one should come from disk again because the data chunks
    // inside the disk_reader buffer are aligned to the store block size.
    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), c + 2, 4);
    ::memset(tmp.data() + 4, c + 3, tmp.size() - 4);
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    // Read the last 4 bytes. We should be done after that
    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), 4));
    ::memset(tmp.data(), c + 3, 4);
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_EQUAL(rdr.curr_disk_offset(), rdr.end_disk_offset());
}

BOOST_AUTO_TEST_CASE(skip_read)
{
    disk_reader rdr(boost::container::string{fname.c_str()}, beg_offs,
                    end_offs);

    std::array<uint8_t, 2_KB> buf, tmp;

    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), 'a', sizeof(tmp));
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    rdr.set_next_offset(4_KB);
    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), 'b', sizeof(tmp));
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    rdr.set_next_offset(3 * 4_KB);
    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), 'd', sizeof(tmp));
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_NE(rdr.curr_disk_offset(), rdr.end_disk_offset());

    BOOST_REQUIRE_NO_THROW(rdr.read(buf.data(), buf.size()));
    ::memset(tmp.data(), 'd', sizeof(tmp));
    BOOST_CHECK(buf == tmp);
    BOOST_CHECK_EQUAL(rdr.curr_disk_offset(), rdr.end_disk_offset());
}

BOOST_AUTO_TEST_SUITE_END()
