#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/fs_table.h"
#include "../../cache/disk_reader.h"
#include "../../cache/memory_writer.h"
#include "../../cache/volume_fd.h"

using namespace cache::detail;

namespace
{

fs_node_key_t gen_key(const char* s) noexcept
{
    return fs_node_key_t{s, strlen(s)};
}

constexpr volume_blocks64_t operator""_vblocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_blocks(v);
}

bool touch_file(const std::string& fpath) noexcept
{
    std::ofstream of(fpath);
    return !of.fail();
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(fs_table_tests)

BOOST_AUTO_TEST_CASE(construct)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 0);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0);
    BOOST_CHECK_EQUAL(fs_table::max_full_size(disk_space, min_obj_size),
                      tbl.max_size_on_disk());
}

BOOST_AUTO_TEST_CASE(add_entry)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1 = gen_key("aaa");
    const auto key2 = gen_key("bbb");

    auto res = tbl.add_entry(key1, make_range_elem(0, 20_KB, 32_vblocks),
                             overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0); // The range is added in-place
    res = tbl.add_entry(key1, make_range_elem(20_KB, 20_KB, 64_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
    res = tbl.add_entry(key1, make_range_elem(60_KB, 20_KB, 96_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 3);
    res = tbl.add_entry(key2, make_range_elem(60_KB, 20_KB, 96_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 3); // SBO - the count is the same

    res = tbl.add_entry(key1, make_range_elem(40_KB - 1, 20_KB + 2, 64_vblocks),
                        [](const auto& rngs, const auto& my_rng)
                        {
                            // Overlaps with these
                            BOOST_CHECK_EQUAL(rngs.size(), 2);
                            BOOST_CHECK_EQUAL(rngs.front().rng_offset(), 20_KB);
                            BOOST_CHECK_EQUAL(rngs.back().rng_offset(), 60_KB);
                            // My range_elem
                            BOOST_CHECK_EQUAL(my_rng.rng_offset(), 40_KB - 1);
                            return true; // Overwrite
                        });
    BOOST_CHECK(res == fs_table::add_res::overwrote);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);

    res = tbl.add_entry(key1, make_range_elem(40_KB, 20_KB, 64_vblocks),
                        [](const auto&, const auto&)
                        {
                            return false; // Don't overwrite
                        });
    BOOST_CHECK(res == fs_table::add_res::skipped);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
}

BOOST_AUTO_TEST_CASE(add_entry_fail_limit_reached)
{
    // Can hold 3 fs_nodes with 1 range, or fewer fs_nodes but with more
    // ranges per node.
    constexpr auto disk_space = 3 * min_obj_size;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1 = gen_key("aaa");
    const auto key2 = gen_key("bbb");
    const auto key3 = gen_key("ccc");

    auto res = tbl.add_entry(key1, make_range_elem(0, 20_KB, 32_vblocks),
                             overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0); // Because of the SBO

    res = tbl.add_entry(key2, make_range_elem(0, 20_KB, 32_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0); // Because of the SBO

    res = tbl.add_entry(key2, make_range_elem(40_KB, 20_KB, 64_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2); // No more SBO

    // Now there won't be space anymore for adding another fs_node
    res = tbl.add_entry(key3, make_range_elem(60_KB, 20_KB, 96_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::limit_reached);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2); // Because of the SBO

    // Now there won't be space anymore for adding new range to the first
    // fs_node, because this would increase the number of ranges with 2,
    // due to the removed SBO effect.
    res = tbl.add_entry(key1, make_range_elem(60_KB, 20_KB, 96_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::limit_reached);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2); // Because of the SBO

    // We reach the limits with the previous range_elem.
    // And we can't add anything more, even range_elem to the second key.
    res = tbl.add_entry(key2, make_range_elem(80_KB, 20_KB, 128_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::limit_reached);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
}

BOOST_AUTO_TEST_CASE(rem_entry)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 5);

    bool rres = tbl.rem_entry(key1, rng11);
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 4);
    rres = tbl.rem_entry(key1, rng12);
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2); // One less because of the SBO
    // Trying to remove already removed range must fail
    rres = tbl.rem_entry(key1, rng12);
    BOOST_CHECK(!rres);
    // The fs_node must be removed after this call
    rres = tbl.rem_entry(key1, rng13);
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
    // Trying to remove already removed key must fail
    rres = tbl.rem_entry(key1, rng13);
    BOOST_CHECK(!rres);

    rres = tbl.rem_entry(key2, rng22);
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0); // One less because of the SBO
    rres = tbl.rem_entry(key2, rng21);
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 0);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0); // One less because of the SBO
}

BOOST_AUTO_TEST_CASE(rem_entries)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 2);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 5);

    auto cnt_removed =
        tbl.rem_entries(key2, [](range_vector& rvec)
                        {
                            auto rngs = rvec.find_exact_range(range{0, 40_KB});
                            BOOST_REQUIRE_EQUAL(rngs.size(), 2);
                            rvec.rem_range(rngs);
                        });
    BOOST_CHECK_EQUAL(cnt_removed, 2);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 3);
    cnt_removed = tbl.rem_entries(
        key2, [](range_vector&)
        {
            BOOST_REQUIRE_MESSAGE(false, "Should not be called, because the "
                                         "key should be no longer present");
        });
    BOOST_CHECK_EQUAL(cnt_removed, 0);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 3);

    cnt_removed =
        tbl.rem_entries(key1, [](range_vector& rvec)
                        {
                            auto rngs = rvec.find_exact_range(range{0, 40_KB});
                            BOOST_REQUIRE_EQUAL(rngs.size(), 2);
                            rvec.rem_range(rngs);
                        });
    BOOST_CHECK_EQUAL(cnt_removed, 2);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0); // The SBO must be activated
    // The fs_table must become empty after this call
    cnt_removed = tbl.rem_entries(
        key1, [](range_vector& rvec)
        {
            auto rngs = rvec.find_exact_range(range{40_KB, 20_KB});
            BOOST_REQUIRE_EQUAL(rngs.size(), 1);
            rvec.rem_range(rngs);
        });
    BOOST_CHECK_EQUAL(cnt_removed, 1);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 0);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0);
}

BOOST_AUTO_TEST_CASE(add_rem_entries_correct_cnt_ranges)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };
    auto do_overwrite = [](const auto&, const auto&)
    {
        return true;
    };

    // Only one key is tested because the range counting peculiarities can
    // be tested over single fs_node.
    const auto key = gen_key("aaa");
    // New fs_node. The range will go in-place (SBO).
    auto res = tbl.add_entry(key, make_range_elem(0, 20_KB, 32_vblocks),
                             overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0);
    // Adding another range will provoke both ranges to go on the heap
    // and thus removing the SBO effect
    res = tbl.add_entry(key, make_range_elem(20_KB, 20_KB, 64_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
    // Overwriting the two existing ranges with a new one, should provoke
    // the SBO to be triggered again.
    res = tbl.add_entry(key, make_range_elem(10_KB, 20_KB, 64_vblocks),
                        do_overwrite);
    BOOST_CHECK(res == fs_table::add_res::overwrote);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0);
    // Adding another range will provoke both ranges to go on the heap
    // and thus removing the SBO effect
    res = tbl.add_entry(key, make_range_elem(40_KB, 20_KB, 96_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
    // Add another range
    res = tbl.add_entry(key, make_range_elem(60_KB, 20_KB, 128_vblocks),
                        overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 3);
    // Overwrite the last two ranges
    res = tbl.add_entry(key, make_range_elem(50_KB, 20_KB, 128_vblocks),
                        do_overwrite);
    BOOST_CHECK(res == fs_table::add_res::overwrote);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 2);
    // Remove the second range. The SBO must kick-in.
    bool rres = tbl.rem_entry(key, make_range_elem(50_KB, 20_KB, 128_vblocks));
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 1);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0);
    // Remove the last range
    rres = tbl.rem_entry(key, make_range_elem(10_KB, 20_KB, 128_vblocks));
    BOOST_CHECK(rres);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), 0);
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), 0);
}

BOOST_AUTO_TEST_CASE(copy_construct)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    fs_table tbl2(tbl);
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), tbl2.cnt_fs_nodes());
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), tbl2.cnt_ranges());

    range_vector rv, rv2;
    tbl.modify_entries(key1, [&](const range_vector& v) mutable
                       {
                           rv = v;
                       });
    tbl2.modify_entries(key1, [&](const range_vector& v) mutable
                        {
                            rv2 = v;
                        });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));

    tbl.modify_entries(key2, [&](const range_vector& v) mutable
                       {
                           rv = v;
                       });
    tbl2.modify_entries(key2, [&](const range_vector& v) mutable
                        {
                            rv2 = v;
                        });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));
}

BOOST_AUTO_TEST_CASE(move_construct)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    range_vector rv, rv2;
    tbl.modify_entries(key1, [&](const range_vector& v) mutable
                       {
                           rv = v;
                       });
    BOOST_REQUIRE(!rv.empty());
    tbl.modify_entries(key2, [&](const range_vector& v) mutable
                       {
                           rv2 = v;
                       });
    BOOST_REQUIRE(!rv2.empty());
    const auto cnt_fs_nodes = tbl.cnt_fs_nodes();
    const auto cnt_ranges   = tbl.cnt_ranges();

    fs_table tbl2(std::move(tbl));
    BOOST_CHECK_EQUAL(0, tbl.cnt_fs_nodes());
    BOOST_CHECK_EQUAL(0, tbl.cnt_ranges());
    BOOST_CHECK_EQUAL(cnt_fs_nodes, tbl2.cnt_fs_nodes());
    BOOST_CHECK_EQUAL(cnt_ranges, tbl2.cnt_ranges());

    bool rres = tbl2.modify_entries(
        key1, [&](const range_vector& v)
        {
            BOOST_REQUIRE_EQUAL(v.size(), rv.size());
            BOOST_CHECK(std::equal(v.begin(), v.end(), rv.begin()));
        });
    BOOST_CHECK(rres);
    rres = tbl2.modify_entries(
        key2, [&](const range_vector& v)
        {
            BOOST_REQUIRE_EQUAL(v.size(), rv2.size());
            BOOST_CHECK(std::equal(v.begin(), v.end(), rv2.begin()));
        });
    BOOST_CHECK(rres);
}

BOOST_AUTO_TEST_CASE(move_assign)
{
    constexpr auto disk_space = 1_MB;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    range_vector rv, rv2;
    tbl.modify_entries(key1, [&](const range_vector& v) mutable
                       {
                           rv = v;
                       });
    BOOST_REQUIRE(!rv.empty());
    tbl.modify_entries(key2, [&](const range_vector& v) mutable
                       {
                           rv2 = v;
                       });
    BOOST_REQUIRE(!rv2.empty());
    const auto cnt_fs_nodes = tbl.cnt_fs_nodes();
    const auto cnt_ranges   = tbl.cnt_ranges();

    fs_table tbl2(disk_space, min_obj_size);
    tbl2 = std::move(tbl);
    BOOST_CHECK_EQUAL(0, tbl.cnt_fs_nodes());
    BOOST_CHECK_EQUAL(0, tbl.cnt_ranges());
    BOOST_CHECK_EQUAL(cnt_fs_nodes, tbl2.cnt_fs_nodes());
    BOOST_CHECK_EQUAL(cnt_ranges, tbl2.cnt_ranges());

    bool rres = tbl2.modify_entries(
        key1, [&](const range_vector& v)
        {
            BOOST_REQUIRE_EQUAL(v.size(), rv.size());
            BOOST_CHECK(std::equal(v.begin(), v.end(), rv.begin()));
        });
    BOOST_CHECK(rres);
    rres = tbl2.modify_entries(
        key2, [&](const range_vector& v)
        {
            BOOST_REQUIRE_EQUAL(v.size(), rv2.size());
            BOOST_CHECK(std::equal(v.begin(), v.end(), rv2.begin()));
        });
    BOOST_CHECK(rres);
}

// Not the usual unit tests, because this one involves file IO
BOOST_AUTO_TEST_CASE(save_load_success)
{
    // Test setup ////////////////////////////////////////
    constexpr auto disk_space = 5 * min_obj_size;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Write in the memmory ////////////////////////////////////////
    auto buf = alloc_page_aligned(store_block_size);
    memory_writer wr(buf.get(), store_block_size);
    tbl.save(wr);
    BOOST_CHECK_EQUAL(wr.written(), tbl.size_on_disk());

    // Write to a file ////////////////////////////////////////
    const std::string fname = "/tmp/fs_table_tests";
    BOOST_REQUIRE(touch_file(fname));
    err_code_t err;
    volume_fd fd;
    fd.open(fname.c_str(), err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to open '" + fname + "'. " + err.message());
    fd.truncate(0, err);
    BOOST_REQUIRE_MESSAGE(!err, "Unable to truncate '" + fname + "'. " +
                                    err.message());
    fd.write(buf.get(), round_to_store_block_size(wr.written()), 0 /*offset*/,
             err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to write '" + fname + "'. " + err.message());
    fd.close(err);

    // Load the table ////////////////////////////////////////
    fs_table tbl2(disk_space, min_obj_size);
    try
    {
        disk_reader rdr(boost::container::string{fname.c_str()}, 0,
                        round_to_store_block_size(wr.written()));

        fs_table::err_info_t err;
        bool rres = tbl2.load(rdr, err);
        BOOST_REQUIRE_MESSAGE(rres, err.to_string());
    }
    catch (const bsys::system_error& err)
    {
        BOOST_REQUIRE_MESSAGE(false, "Unable to load fs_table from '" + fname +
                                         "'. " + err.code().message());
        return;
    }

    // Now compare the tables ////////////////////////////////////////
    BOOST_CHECK_EQUAL(tbl.cnt_fs_nodes(), tbl2.cnt_fs_nodes());
    BOOST_CHECK_EQUAL(tbl.cnt_ranges(), tbl2.cnt_ranges());

    range_vector rv, rv2;
    tbl.modify_entries(key1, [&](const range_vector& v) mutable
                       {
                           rv = v;
                       });
    tbl2.modify_entries(key1, [&](const range_vector& v) mutable
                        {
                            rv2 = v;
                        });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));

    tbl.modify_entries(key2, [&](const range_vector& v) mutable
                       {
                           rv = v;
                       });
    tbl2.modify_entries(key2, [&](const range_vector& v) mutable
                        {
                            rv2 = v;
                        });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));
}

BOOST_AUTO_TEST_CASE(save_load_fail_first_sentinel)
{
    // Test setup ////////////////////////////////////////
    constexpr auto disk_space = 5 * min_obj_size;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Write in the memmory ////////////////////////////////////////
    auto buf = alloc_page_aligned(store_block_size);
    memory_writer wr(buf.get(), store_block_size);
    tbl.save(wr);
    BOOST_CHECK_EQUAL(wr.written(), tbl.size_on_disk());

    // Corrupt the header sentinel value ///////////////////////////////////////
    buf[0] = 0xFF;

    // Write to a file ////////////////////////////////////////
    const std::string fname = "/tmp/fs_table_tests";
    BOOST_REQUIRE(touch_file(fname));
    err_code_t err;
    volume_fd fd;
    fd.open(fname.c_str(), err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to open '" + fname + "'. " + err.message());
    fd.truncate(0, err);
    BOOST_REQUIRE_MESSAGE(!err, "Unable to truncate '" + fname + "'. " +
                                    err.message());
    fd.write(buf.get(), round_to_store_block_size(wr.written()), 0 /*offset*/,
             err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to write '" + fname + "'. " + err.message());
    fd.close(err);

    // Load the table ////////////////////////////////////////
    fs_table tbl2(disk_space, min_obj_size);
    try
    {
        disk_reader rdr(boost::container::string{fname.c_str()}, 0,
                        round_to_store_block_size(wr.written()));

        fs_table::err_info_t err;
        bool rres = tbl2.load(rdr, err);
        BOOST_REQUIRE(!rres);
        BOOST_TEST_MESSAGE(err.to_string());
    }
    catch (const bsys::system_error& err)
    {
        BOOST_REQUIRE_MESSAGE(false, "Unable to load fs_table from '" + fname +
                                         "'. " + err.code().message());
    }
}

BOOST_AUTO_TEST_CASE(save_load_fail_cnt_fs_nodes)
{
    // Test setup ////////////////////////////////////////
    constexpr auto disk_space = 5 * min_obj_size;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Write in the memmory ////////////////////////////////////////
    auto buf = alloc_page_aligned(store_block_size);
    memory_writer wr(buf.get(), store_block_size);
    tbl.save(wr);
    BOOST_CHECK_EQUAL(wr.written(), tbl.size_on_disk());

    // Corrupt the fs_nodes count value ///////////////////////////////////////
    buf[8 + 7] = 0xFF;

    // Write to a file ////////////////////////////////////////
    const std::string fname = "/tmp/fs_table_tests";
    BOOST_REQUIRE(touch_file(fname));
    err_code_t err;
    volume_fd fd;
    fd.open(fname.c_str(), err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to open '" + fname + "'. " + err.message());
    fd.truncate(0, err);
    BOOST_REQUIRE_MESSAGE(!err, "Unable to truncate '" + fname + "'. " +
                                    err.message());
    fd.write(buf.get(), round_to_store_block_size(wr.written()), 0 /*offset*/,
             err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to write '" + fname + "'. " + err.message());
    fd.close(err);

    // Load the table ////////////////////////////////////////
    fs_table tbl2(disk_space, min_obj_size);
    try
    {
        disk_reader rdr(boost::container::string{fname.c_str()}, 0,
                        round_to_store_block_size(wr.written()));

        fs_table::err_info_t err;
        bool rres = tbl2.load(rdr, err);
        BOOST_REQUIRE(!rres);
        BOOST_TEST_MESSAGE(err.to_string());
    }
    catch (const bsys::system_error& err)
    {
        BOOST_REQUIRE_MESSAGE(false, "Unable to load fs_table from '" + fname +
                                         "'. " + err.code().message());
    }
}

BOOST_AUTO_TEST_CASE(save_load_fail_random_curruption)
{
    // Test setup ////////////////////////////////////////
    constexpr auto disk_space = 5 * min_obj_size;
    fs_table tbl(disk_space, min_obj_size);

    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = tbl.add_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = tbl.add_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Write in the memmory ////////////////////////////////////////
    auto buf = alloc_page_aligned(store_block_size);
    memory_writer wr(buf.get(), store_block_size);
    tbl.save(wr);
    BOOST_CHECK_EQUAL(wr.written(), tbl.size_on_disk());

    // Corrupt the fs_nodes count value ///////////////////////////////////////
    ::memset(buf.get() + 64, 0xFF, store_block_size - 64);

    // Write to a file ////////////////////////////////////////
    const std::string fname = "/tmp/fs_table_tests";
    BOOST_REQUIRE(touch_file(fname));
    err_code_t err;
    volume_fd fd;
    fd.open(fname.c_str(), err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to open '" + fname + "'. " + err.message());
    fd.truncate(0, err);
    BOOST_REQUIRE_MESSAGE(!err, "Unable to truncate '" + fname + "'. " +
                                    err.message());
    fd.write(buf.get(), round_to_store_block_size(wr.written()), 0 /*offset*/,
             err);
    BOOST_REQUIRE_MESSAGE(!err,
                          "Unable to write '" + fname + "'. " + err.message());
    fd.close(err);

    // Load the table ////////////////////////////////////////
    fs_table tbl2(disk_space, min_obj_size);
    try
    {
        disk_reader rdr(boost::container::string{fname.c_str()}, 0,
                        round_to_store_block_size(wr.written()));

        fs_table::err_info_t err;
        bool rres = tbl2.load(rdr, err);
        BOOST_REQUIRE(!rres);
        BOOST_TEST_MESSAGE(err.to_string());
    }
    catch (const bsys::system_error& err)
    {
        BOOST_REQUIRE_MESSAGE(false, "Unable to load fs_table from '" + fname +
                                         "'. " + err.code().message());
    }
}

BOOST_AUTO_TEST_SUITE_END()
