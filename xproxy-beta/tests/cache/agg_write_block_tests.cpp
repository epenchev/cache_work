#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/agg_write_block.h"
#include "../../cache/cache_stats.h"
#include "../../cache/object_frag_hdr.h"
#include "../../cache/range.h"
#include "../../cache/range_elem.h"

using namespace cache::detail;

namespace
{

auto make_key(char c) noexcept
{
    char cc[16];
    ::memset(cc, c, sizeof(cc));
    return fs_node_key_t{cc, sizeof(cc)};
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(agg_write_block_tests)

BOOST_AUTO_TEST_CASE(add_read_fragment_ok)
{
    constexpr auto cnt_entries = 100;
    constexpr auto elem_size   = 20_KB;
    constexpr auto disk_wr_pos = volume_blocks64_t::create_from_bytes(200_MB);
    using buff_t = std::array<uint8_t, elem_size>;
    std::vector<buff_t> frags(cnt_entries);
    std::vector<range_elem> relems(cnt_entries);

    agg_write_block awb;
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), 0);

    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        auto& frag = frags[i];
        ::memset(frag.data(), (uint8_t)i, frag.size());
        const auto r =
            awb.add_fragment(make_key(i), range{offs, elem_size}, disk_wr_pos,
                             agg_write_block::frag_ro_buff_t{frag});
        BOOST_REQUIRE(r);
        relems[i] = r.value();
    }
    const auto written = cnt_entries * object_frag_size(elem_size);
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size - written);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), written);

    std::array<uint8_t, sizeof(object_frag_hdr) + elem_size> read_buff;
    for (auto i = 0; i < cnt_entries; ++i)
    {
        const auto key = make_key(i);
        const bool r =
            awb.try_read_fragment(key, relems[i], disk_wr_pos,
                                  agg_write_block::frag_wr_buff_t{read_buff});
        BOOST_REQUIRE(r);
        const auto hdr = object_frag_hdr::create(key, relems[i]);
        BOOST_CHECK(::memcmp(read_buff.data(), &hdr, sizeof(hdr)) == 0);
        BOOST_CHECK(
            ::memcmp(&read_buff[sizeof(hdr)], frags[i].data(), elem_size) == 0);
    }
}

BOOST_AUTO_TEST_CASE(add_fragment_fail_overlaps)
{
    constexpr auto elem_size   = 20_KB;
    constexpr auto disk_wr_pos = volume_blocks64_t::create_from_bytes(200_MB);

    using buff_t = std::array<uint8_t, elem_size>;
    buff_t buff;

    agg_write_block awb;
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), 0);

    auto r = awb.add_fragment(make_key(1), range{36_KB, elem_size}, disk_wr_pos,
                              agg_write_block::frag_ro_buff_t{buff});
    BOOST_REQUIRE(r);
    r = awb.add_fragment(make_key(1), range{62_KB, elem_size}, disk_wr_pos,
                         agg_write_block::frag_ro_buff_t{buff});
    BOOST_REQUIRE(r);

    const auto written = 2 * object_frag_size(elem_size);
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size - written);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), written);

    // All cases of overlapping are already tested in the agg_write_meta_tests
    // Overlaps with the beginning of the first.
    r = awb.add_fragment(make_key(1), range{17_KB, elem_size}, disk_wr_pos,
                         agg_write_block::frag_ro_buff_t{buff});
    BOOST_CHECK(r.error() == agg_write_block::fail_res::overlaps);
    // Overlaps with the end of the first and the beginning of the second
    r = awb.add_fragment(make_key(1), range{50_KB, elem_size}, disk_wr_pos,
                         agg_write_block::frag_ro_buff_t{buff});
    BOOST_CHECK(r.error() == agg_write_block::fail_res::overlaps);
    // Overlaps with the end of the second
    r = awb.add_fragment(make_key(1), range{70_KB, elem_size}, disk_wr_pos,
                         agg_write_block::frag_ro_buff_t{buff});
    BOOST_CHECK(r.error() == agg_write_block::fail_res::overlaps);

    // The available bytes/free space must not been changed.
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size - written);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), written);
}

BOOST_AUTO_TEST_CASE(add_fragment_fail_no_space_meta)
{
    // Use min object size so that the metadata space gets exhausted first
    constexpr auto elem_size   = min_obj_size;
    constexpr auto disk_wr_pos = volume_blocks64_t::create_from_bytes(200_MB);
    using buff_t               = std::array<uint8_t, elem_size>;
    buff_t frag;

    agg_write_block awb;
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), 0);

    auto i = 0;
    for (auto offs = 1000_MB;; ++i, offs -= elem_size)
    {
        const auto r =
            awb.add_fragment(make_key(i), range{offs, elem_size}, disk_wr_pos,
                             agg_write_block::frag_ro_buff_t{frag});
        if (!r)
        {
            BOOST_REQUIRE(r.error() ==
                          agg_write_block::fail_res::no_space_meta);
            break;
        }
    }
    BOOST_REQUIRE(i > 0);
    const auto written = i * object_frag_size(elem_size);
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size - written);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), written);
}

BOOST_AUTO_TEST_CASE(add_fragment_fail_no_space_data)
{
    // Use max object size so that the data space gets exhausted first
    constexpr auto elem_size   = range_elem::max_rng_size();
    constexpr auto disk_wr_pos = volume_blocks64_t::create_from_bytes(200_MB);
    using buff_t               = std::array<uint8_t, elem_size>;
    buff_t frag;

    agg_write_block awb;
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), 0);

    auto i = 0;
    for (auto offs = 1000_MB;; ++i, offs -= elem_size)
    {
        const auto r =
            awb.add_fragment(make_key(i), range{offs, elem_size}, disk_wr_pos,
                             agg_write_block::frag_ro_buff_t{frag});
        if (!r)
        {
            BOOST_REQUIRE(r.error() ==
                          agg_write_block::fail_res::no_space_data);
            break;
        }
    }
    BOOST_REQUIRE(i > 0);
    const auto written = i * object_frag_size(elem_size);
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size - written);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), written);
}

BOOST_AUTO_TEST_CASE(begin_end_disk_write)
{
    constexpr auto cnt_entries = 100;
    constexpr auto elem_size   = 20_KB;
    constexpr auto disk_wr_pos = volume_blocks64_t::create_from_bytes(200_MB);
    using buff_t               = std::array<uint8_t, elem_size>;
    buff_t frag;

    agg_write_block awb;
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), 0);

    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        const auto r =
            awb.add_fragment(make_key(i), range{offs, elem_size}, disk_wr_pos,
                             agg_write_block::frag_ro_buff_t{frag});
        BOOST_REQUIRE(r);
    }
    const auto written = cnt_entries * object_frag_size(elem_size);
    BOOST_REQUIRE_EQUAL(awb.free_space(), agg_write_data_size - written);
    BOOST_REQUIRE_EQUAL(awb.bytes_avail(), written);

    cache::stats_fs_wr unused;
    auto ro_buff = awb.begin_disk_write(unused);
    BOOST_REQUIRE_EQUAL(ro_buff.size(), round_to_store_block_size(
                                            agg_write_meta_size + written));
    auto entries = awb.end_disk_write();
    BOOST_REQUIRE_EQUAL(entries.size(), cnt_entries);
}

BOOST_AUTO_TEST_SUITE_END()
