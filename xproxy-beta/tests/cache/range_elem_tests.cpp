#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/range_elem.h"

using namespace cache::detail;

namespace
{

constexpr volume_blocks64_t operator""_vblocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_blocks(v);
}

constexpr volume_blocks64_t bytes2blocks(bytes64_t v) noexcept
{
    return volume_blocks64_t::round_up_to_blocks(v);
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(range_elem_tests)

BOOST_AUTO_TEST_CASE(range_elem_make_zero)
{
    const auto re = make_zero_range_elem();
    BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
    BOOST_CHECK_EQUAL(re.has_readers(), false);
    BOOST_CHECK_EQUAL(re.rng_offset(), 0);
    BOOST_CHECK_EQUAL(re.rng_end_offset(), 0);
    BOOST_CHECK_EQUAL(re.rng_size(), 0);
    BOOST_CHECK_EQUAL(re.disk_offset(), 0_vblocks);
    BOOST_CHECK_EQUAL(re.disk_end_offset(), 0_vblocks);
}

BOOST_AUTO_TEST_CASE(range_elem_check_readers)
{
    constexpr bytes64_t rng_offs = 6_GB;
    constexpr bytes64_t rng_size = range_elem::max_rng_size();
    constexpr auto disk_offs     = 10485760_vblocks;

    auto re = make_range_elem(rng_offs, rng_size, disk_offs);

    BOOST_CHECK_EQUAL(re.cnt_readers(), 0);

    for (auto i = 1U; i <= range_elem::max_cnt_readers(); ++i)
    {
        BOOST_CHECK(re.atomic_inc_readers());
        BOOST_CHECK_EQUAL(re.cnt_readers(), i);
    }

    // This must fail we are at the limit.
    BOOST_CHECK(!re.atomic_inc_readers());
    BOOST_CHECK_EQUAL(re.cnt_readers(), range_elem::max_cnt_readers());

    for (auto i = range_elem::max_cnt_readers(); i > 0; --i)
    {
        re.atomic_dec_readers();
        BOOST_CHECK_EQUAL(re.cnt_readers(), i - 1);
    }
    BOOST_CHECK_EQUAL(re.cnt_readers(), 0);
}

BOOST_AUTO_TEST_CASE(range_elem_make_non_zero)
{
    { // small values
        constexpr bytes64_t rng_offs = 1024;
        constexpr bytes64_t rng_size = 10400;
        constexpr auto disk_offs     = 128_vblocks;

        const auto re = make_range_elem(rng_offs, rng_size, disk_offs);
        BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
        BOOST_CHECK_EQUAL(re.cnt_readers(), 0);
        BOOST_CHECK_EQUAL(re.rng_offset(), rng_offs);
        BOOST_CHECK_EQUAL(re.rng_end_offset(), rng_offs + rng_size);
        BOOST_CHECK_EQUAL(re.rng_size(), rng_size);
        BOOST_CHECK_EQUAL(re.disk_offset(), disk_offs);
        BOOST_CHECK_EQUAL(re.disk_end_offset(),
                          disk_offs + bytes2blocks(rng_size));
    }
    { // big values
        constexpr bytes64_t rng_offs = 6_GB;
        constexpr bytes64_t rng_size = range_elem::max_rng_size() - 1;
        constexpr auto disk_offs     = 104857_vblocks;

        const auto re = make_range_elem(rng_offs, rng_size, disk_offs);
        BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
        BOOST_CHECK_EQUAL(re.has_readers(), false);
        BOOST_CHECK_EQUAL(re.rng_offset(), rng_offs);
        BOOST_CHECK_EQUAL(re.rng_end_offset(), rng_offs + rng_size);
        BOOST_CHECK_EQUAL(re.rng_size(), rng_size);
        BOOST_CHECK_EQUAL(re.disk_offset(), disk_offs);
        BOOST_CHECK_EQUAL(re.disk_end_offset(),
                          disk_offs + bytes2blocks(rng_size));
    }
    { // huge values
        constexpr bytes64_t rng_offs = 7986_MB;
        constexpr bytes64_t rng_size = range_elem::max_rng_size() - 1;
        constexpr auto disk_offs     = 1048576_vblocks;

        const auto re = make_range_elem(rng_offs, rng_size, disk_offs);
        BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
        BOOST_CHECK_EQUAL(re.has_readers(), false);
        BOOST_CHECK_EQUAL(re.rng_offset(), rng_offs);
        BOOST_CHECK_EQUAL(re.rng_end_offset(), rng_offs + rng_size);
        BOOST_CHECK_EQUAL(re.rng_size(), rng_size);
        BOOST_CHECK_EQUAL(re.disk_offset(), disk_offs);
        BOOST_CHECK_EQUAL(re.disk_end_offset(),
                          disk_offs + bytes2blocks(rng_size));
    }
}

BOOST_AUTO_TEST_CASE(range_elem_make_read)
{
    constexpr bytes64_t rng_offs = 4345_MB;
    constexpr bytes64_t rng_size = range_elem::max_rng_size();
    constexpr auto disk_offs     = 104854_vblocks;

    range_elem re;
    // Check initial state
    re.set_mark();
    BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
    re.reset_meta();
    BOOST_CHECK_EQUAL(re.has_readers(), false);

    // Set as read and set all values to something then check that
    // everything set is correct
    re.atomic_inc_readers();
    re.set_rng_offset(rng_offs);
    re.set_rng_size(rng_size);
    re.set_disk_offset(disk_offs);
    BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
    BOOST_CHECK_EQUAL(re.has_readers(), true);
    BOOST_CHECK_EQUAL(re.rng_offset(), rng_offs);
    BOOST_CHECK_EQUAL(re.rng_end_offset(), rng_offs + rng_size);
    BOOST_CHECK_EQUAL(re.rng_size(), rng_size);
    BOOST_CHECK_EQUAL(re.disk_offset(), disk_offs);
    BOOST_CHECK_EQUAL(re.disk_end_offset(), disk_offs + bytes2blocks(rng_size));

    // Reset all values to something else and check that everything
    // is the way it should be.
    re.set_rng_offset(0);
    re.set_rng_size(range_elem::min_rng_size());
    re.set_disk_offset(128_vblocks);
    BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
    BOOST_CHECK_EQUAL(re.has_readers(), true);
    BOOST_CHECK_EQUAL(re.rng_offset(), 0);
    BOOST_CHECK_EQUAL(re.rng_end_offset(), range_elem::min_rng_size());
    BOOST_CHECK_EQUAL(re.rng_size(), range_elem::min_rng_size());
    BOOST_CHECK_EQUAL(re.disk_offset(), 128_vblocks);
    BOOST_CHECK_EQUAL(re.disk_end_offset(),
                      128_vblocks + bytes2blocks(range_elem::min_rng_size()));

    // Clear the read flag and check that everything is ok
    re.atomic_dec_readers();
    BOOST_CHECK_EQUAL(range_elem::is_range_elem(&re), true);
    BOOST_CHECK_EQUAL(re.has_readers(), false);
    BOOST_CHECK_EQUAL(re.rng_offset(), 0);
    BOOST_CHECK_EQUAL(re.rng_end_offset(), range_elem::min_rng_size());
    BOOST_CHECK_EQUAL(re.rng_size(), range_elem::min_rng_size());
    BOOST_CHECK_EQUAL(re.disk_offset(), 128_vblocks);
    BOOST_CHECK_EQUAL(re.disk_end_offset(),
                      128_vblocks + bytes2blocks(range_elem::min_rng_size()));
}

BOOST_AUTO_TEST_SUITE_END()
