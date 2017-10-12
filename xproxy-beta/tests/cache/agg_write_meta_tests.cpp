#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/agg_write_meta.h"
#include "../../cache/memory_reader.h"
#include "../../cache/memory_writer.h"
#include "../../cache/range_elem.h"

using namespace cache::detail;

namespace
{

constexpr volume_blocks64_t operator""_vblocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_blocks(v);
}

auto make_key(char c) noexcept
{
    char cc[16];
    ::memset(cc, c, sizeof(cc));
    return fs_node_key_t{cc, sizeof(cc)};
}

auto make_relem(bytes64_t offs, bytes32_t size) noexcept
{
    return make_range_elem(
        offs, size, volume_blocks64_t::round_up_to_blocks(128_KB + offs));
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(agg_write_meta_tests)

BOOST_AUTO_TEST_CASE(construct)
{
    agg_write_meta awm(4_KB);
    BOOST_CHECK(awm.empty());
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 0);
    BOOST_CHECK_EQUAL(awm.max_cnt_entries(), 4_KB / sizeof(agg_meta_entry) - 1);
}

BOOST_AUTO_TEST_CASE(add_entry_ok)
{
    const auto cnt_entries    = 100;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        auto r = awm.add_entry(make_key(i), make_relem(offs, 20_KB));
        BOOST_CHECK(r == agg_write_meta::add_res::ok);
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 100);
    offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        auto r = awm.has_entry(make_key(i), make_relem(offs, 20_KB));
        BOOST_CHECK(r);
    }
}

BOOST_AUTO_TEST_CASE(add_entry_overlaps_begin)
{
    const auto cnt_entries    = 10;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto r = awm.add_entry(make_key('a'), make_relem(50_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(70_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(90_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
    r = awm.add_entry(make_key('a'), make_relem(30_KB + 1, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::overlaps);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
}

BOOST_AUTO_TEST_CASE(add_entry_overlaps_end)
{
    const auto cnt_entries    = 10;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto r = awm.add_entry(make_key('a'), make_relem(50_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(90_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(120_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
    r = awm.add_entry(make_key('a'), make_relem(140_KB - 1, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::overlaps);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
}

BOOST_AUTO_TEST_CASE(add_entry_overlaps_mid)
{
    const auto cnt_entries    = 10;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto r = awm.add_entry(make_key('a'), make_relem(50_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(90_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(120_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
    r = awm.add_entry(make_key('a'), make_relem(110_KB - 1, 20_KB + 2));
    BOOST_REQUIRE(r == agg_write_meta::add_res::overlaps);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
}

BOOST_AUTO_TEST_CASE(add_entry_no_space)
{
    const auto cnt_entries    = 3;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto r = awm.add_entry(make_key('a'), make_relem(50_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(90_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(make_key('a'), make_relem(120_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
    r = awm.add_entry(make_key('a'), make_relem(150_KB, 20_KB));
    BOOST_REQUIRE(r == agg_write_meta::add_res::no_space);
    BOOST_CHECK_EQUAL(awm.cnt_entries(), 3);
}

BOOST_AUTO_TEST_CASE(rem_entry)
{
    const auto cnt_entries    = 5;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);

    std::pair<fs_node_key_t, range_elem> arr[] = {
        std::make_pair(make_key('a'), make_relem(500_KB, 20_KB)),
        std::make_pair(make_key('b'), make_relem(90_KB, 20_KB)),
        std::make_pair(make_key('c'), make_relem(20_KB, 20_KB)),
        std::make_pair(make_key('c'), make_relem(40_KB, 20_KB))};

    agg_write_meta awm(meta_buff_size);
    auto r = awm.add_entry(arr[0].first, arr[0].second);
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(arr[1].first, arr[1].second);
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(arr[2].first, arr[2].second);
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);
    r = awm.add_entry(arr[3].first, arr[3].second);
    BOOST_REQUIRE(r == agg_write_meta::add_res::ok);

    auto rr = awm.has_entry(arr[0].first, arr[0].second);
    BOOST_REQUIRE(rr);
    rr = awm.has_entry(arr[1].first, arr[1].second);
    BOOST_REQUIRE(rr);
    rr = awm.has_entry(arr[2].first, arr[2].second);
    BOOST_REQUIRE(rr);
    rr = awm.has_entry(arr[3].first, arr[3].second);
    BOOST_REQUIRE(rr);
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 4);

    for (auto it = awm.begin(); it != awm.end();)
    {
        auto e = *it;
        it = awm.rem_entry(it);
        BOOST_REQUIRE(!awm.has_entry(e.key(), e.rng()));
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 0);
}

BOOST_AUTO_TEST_CASE(load_success)
{
    const auto cnt_entries    = 100;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        awm.add_entry(make_key(i), make_relem(offs, 20_KB));
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 100);

    auto buff = std::make_unique<uint8_t[]>(meta_buff_size);
    {
        memory_writer wr(buff.get(), meta_buff_size);
        awm.save(wr);
    }

    agg_write_meta awm2(meta_buff_size);
    {
        memory_reader rd(buff.get(), meta_buff_size);
        const bool res = awm2.load(rd);
        BOOST_REQUIRE(res);
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), awm2.cnt_entries());
    BOOST_CHECK(std::equal(awm.begin(), awm.end(), awm2.begin()));
}

BOOST_AUTO_TEST_CASE(load_fail_wrong_magic_hdr)
{
    const auto cnt_entries    = 100;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        awm.add_entry(make_key(i), make_relem(offs, 20_KB));
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 100);

    auto buff = std::make_unique<uint8_t[]>(meta_buff_size);
    {
        memory_writer wr(buff.get(), meta_buff_size);
        awm.save(wr);
    }

    // Corrupt the header.
    ::memset(buff.get(), 0xFF, 8);

    agg_write_meta awm2(meta_buff_size);
    {
        memory_reader rd(buff.get(), meta_buff_size);
        const bool res = awm2.load(rd);
        BOOST_REQUIRE(!res);
    }
    BOOST_REQUIRE_EQUAL(0, awm2.cnt_entries());
}

BOOST_AUTO_TEST_CASE(load_fail_wrong_cnt_entries)
{
    const auto cnt_entries    = 100;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        awm.add_entry(make_key(i), make_relem(offs, 20_KB));
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 100);

    auto buff = std::make_unique<uint8_t[]>(meta_buff_size);
    {
        memory_writer wr(buff.get(), meta_buff_size);
        awm.save(wr);
    }

    // Corrupt the entries count
    ::memset(buff.get() + 8, 0xFF, 4);

    agg_write_meta awm2(meta_buff_size);
    {
        memory_reader rd(buff.get(), meta_buff_size);
        const bool res = awm2.load(rd);
        BOOST_REQUIRE(!res);
    }
    BOOST_REQUIRE_EQUAL(0, awm2.cnt_entries());
}

BOOST_AUTO_TEST_CASE(load_fail_not_sorted_entries)
{
    const auto cnt_entries    = 100;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        awm.add_entry(make_key(i), make_relem(offs, 20_KB));
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 100);

    auto buff = std::make_unique<uint8_t[]>(meta_buff_size);
    {
        memory_writer wr(buff.get(), meta_buff_size);
        awm.save(wr);
    }

    // Corrupt the first entry and thus make the collection unsorted
    ::memset(buff.get() + 12, 0xFF, sizeof(agg_meta_entry));

    agg_write_meta awm2(meta_buff_size);
    {
        memory_reader rd(buff.get(), meta_buff_size);
        const bool res = awm2.load(rd);
        BOOST_REQUIRE(!res);
    }
    BOOST_REQUIRE_EQUAL(0, awm2.cnt_entries());
}

BOOST_AUTO_TEST_CASE(load_fail_wrong_magic_ftr)
{
    const auto cnt_entries    = 100;
    const auto meta_buff_size = (cnt_entries + 1) * sizeof(agg_meta_entry);
    agg_write_meta awm(meta_buff_size);
    auto offs = 200_MB;
    for (auto i = 0; i < cnt_entries; ++i, offs -= 30_KB)
    {
        awm.add_entry(make_key(i), make_relem(offs, 20_KB));
    }
    BOOST_REQUIRE_EQUAL(awm.cnt_entries(), 100);

    auto buff = std::make_unique<uint8_t[]>(meta_buff_size);
    {
        memory_writer wr(buff.get(), meta_buff_size);
        awm.save(wr);
    }

    // Corrupt the footer
    auto ftr_pos = buff.get() + meta_buff_size - sizeof(agg_meta_entry) +
                   8 /*hdr*/ + 4 /*cnt entries*/;
    ::memset(ftr_pos, 0xFF, 8);

    agg_write_meta awm2(meta_buff_size);
    {
        memory_reader rd(buff.get(), meta_buff_size);
        const bool res = awm2.load(rd);
        BOOST_REQUIRE(!res);
    }
    BOOST_REQUIRE_EQUAL(0, awm2.cnt_entries());
}

BOOST_AUTO_TEST_SUITE_END()
