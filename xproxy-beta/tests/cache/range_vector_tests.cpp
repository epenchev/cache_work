#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/range_vector.h"
#include "../../cache/memory_reader.h"
#include "../../cache/memory_writer.h"

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

BOOST_AUTO_TEST_SUITE(range_vector_tests)

BOOST_AUTO_TEST_CASE(default_construct)
{
    range_vector rv;
    BOOST_CHECK(rv.empty());
    BOOST_CHECK(!rv.data());
    BOOST_CHECK_EQUAL(rv.size(), 0);
    BOOST_CHECK_EQUAL(rv.begin(), rv.end());
    BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());
}

BOOST_AUTO_TEST_CASE(single_element)
{
    constexpr bytes64_t rng_offs = 1024;
    constexpr bytes64_t rng_size = min_obj_size + 2048;
    constexpr auto disk_offs     = 32_vblocks;

    const auto re = make_range_elem(rng_offs, rng_size, disk_offs);

    auto check_after_add = [](const range_vector& rv, const range_elem& re)
    {
        BOOST_CHECK(!rv.empty());
        BOOST_CHECK_EQUAL((void*)rv.data(), (void*)&rv); // Check the SBO
        BOOST_CHECK_EQUAL(*rv.data(), re);
        BOOST_CHECK_EQUAL(rv.size(), 1);
        BOOST_CHECK_EQUAL(*rv.begin(), re);
        BOOST_CHECK_EQUAL(*rv.cbegin(), re);
        BOOST_CHECK_NE(rv.begin(), rv.end());
        BOOST_CHECK_NE(rv.cbegin(), rv.cend());
    };

    auto check_after_rem = [](const range_vector& rv)
    {
        BOOST_CHECK(rv.empty());
        BOOST_CHECK(!rv.data());
        BOOST_CHECK_EQUAL(rv.size(), 0);
        BOOST_CHECK_EQUAL(rv.begin(), rv.end());
        BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());
    };

    {
        // Add single element
        range_vector rv;
        const auto ret = rv.add_range(re);
        BOOST_REQUIRE(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
        check_after_add(rv, re);

        // Remove the element. The container should become empty
        const auto it = rv.rem_range(ret.first);
        BOOST_CHECK_EQUAL(it, rv.end());
        BOOST_CHECK_EQUAL(it, rv.cend());
        check_after_rem(rv);
    }
    {
        // Create single element
        range_vector rv(re);
        check_after_add(rv, re);

        // Remove the element. The container should become empty
        const auto it = rv.rem_range(rv.begin());
        BOOST_CHECK_EQUAL(it, rv.end());
        BOOST_CHECK_EQUAL(it, rv.cend());
        check_after_rem(rv);
    }
}

BOOST_AUTO_TEST_CASE(two_elements)
{
    const auto re1 = make_range_elem(1024, min_obj_size + 1024, 32_vblocks);
    const auto re2 = make_range_elem(10240, min_obj_size + 2, 20_vblocks);

    // Add the second element first
    range_vector rv(re2);
    {
        // Add the first element second
        const auto ret = rv.add_range(re1);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re1);
    }
    BOOST_CHECK_EQUAL(rv.size(), 2);
    // The elements must be sorted and the second added element must be first
    BOOST_CHECK(std::is_sorted(rv.begin(), rv.end()));
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    BOOST_CHECK_EQUAL(*rv.begin(), re1);
    BOOST_CHECK_EQUAL(*(rv.begin() + 1), re2);

    // Erase the first element and make sure that the
    // SBO has kicked in, because there must be only 1 element.
    auto it = rv.rem_range(rv.begin());
    BOOST_CHECK_EQUAL(it, rv.begin());
    BOOST_CHECK_EQUAL(it, rv.data());
    BOOST_CHECK_EQUAL(*it, re2);
    BOOST_CHECK_EQUAL(rv.size(), 1);
    // Check the Small Buffer Optimization
    BOOST_CHECK_EQUAL((void*)rv.data(), (void*)&rv);
    BOOST_CHECK_EQUAL(*rv.data(), re2);

    // Remove the last element. The container should become empty
    it = rv.rem_range(it);
    BOOST_CHECK(rv.empty());
    BOOST_CHECK(!rv.data());
    BOOST_CHECK_EQUAL(rv.size(), 0);
    BOOST_CHECK_EQUAL(rv.begin(), rv.end());
    BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());

    // Now add two elements and remove both of them at once
    BOOST_REQUIRE(rv.add_range(re1).second);
    BOOST_REQUIRE(rv.add_range(re2).second);
    BOOST_CHECK(std::is_sorted(rv.begin(), rv.end()));
    BOOST_CHECK_EQUAL(*rv.begin(), re1);
    BOOST_CHECK_EQUAL(*(rv.begin() + 1), re2);

    it = rv.rem_range(range_vector::iter_range(rv.begin(), rv.end()));
    BOOST_CHECK(it == rv.end());
    BOOST_CHECK(rv.empty());
    BOOST_CHECK(!rv.data());
}

BOOST_AUTO_TEST_CASE(five_elements_rem_first_two)
{
    const auto re1 = make_range_elem(1024, min_obj_size + 1024, 32_vblocks);
    const auto re2 = make_range_elem(10240, min_obj_size + 2, 20_vblocks);
    const auto re3 = make_range_elem(10240 * 2, min_obj_size + 2, 20_vblocks);
    const auto re4 = make_range_elem(10240 * 3, min_obj_size + 2, 20_vblocks);
    const auto re5 = make_range_elem(10240 * 4, min_obj_size + 2, 20_vblocks);

    range_vector rv;
    BOOST_REQUIRE(rv.add_range(re5).second);
    BOOST_REQUIRE(rv.add_range(re2).second);
    BOOST_REQUIRE(rv.add_range(re3).second);
    BOOST_REQUIRE(rv.add_range(re1).second);
    BOOST_REQUIRE(rv.add_range(re4).second);
    BOOST_CHECK(std::is_sorted(rv.begin(), rv.end()));

    auto it = rv.rem_range(boost::make_iterator_range_n(rv.begin(), 2));
    BOOST_REQUIRE_EQUAL(it, rv.begin());
    BOOST_REQUIRE_EQUAL(rv.size(), 3);
    BOOST_CHECK_EQUAL(*(rv.begin()), re3);
    BOOST_CHECK_EQUAL(*(rv.begin() + 1), re4);
    BOOST_CHECK_EQUAL(*(rv.begin() + 2), re5);

    it = rv.rem_range(boost::make_iterator_range_n(rv.begin(), 2));
    BOOST_REQUIRE_EQUAL(rv.size(), 1);
    BOOST_REQUIRE_EQUAL(it, rv.begin());
    BOOST_CHECK_EQUAL(*(rv.begin()), re5);
}

BOOST_AUTO_TEST_CASE(five_elements_rem_mid_elems)
{
    const auto re1 = make_range_elem(1024, min_obj_size + 1024, 32_vblocks);
    const auto re2 = make_range_elem(10240, min_obj_size + 2, 20_vblocks);
    const auto re3 = make_range_elem(10240 * 2, min_obj_size + 2, 20_vblocks);
    const auto re4 = make_range_elem(10240 * 3, min_obj_size + 2, 20_vblocks);
    const auto re5 = make_range_elem(10240 * 4, min_obj_size + 2, 20_vblocks);

    range_vector rv;
    BOOST_REQUIRE(rv.add_range(re5).second);
    BOOST_REQUIRE(rv.add_range(re2).second);
    BOOST_REQUIRE(rv.add_range(re3).second);
    BOOST_REQUIRE(rv.add_range(re1).second);
    BOOST_REQUIRE(rv.add_range(re4).second);
    BOOST_CHECK(std::is_sorted(rv.begin(), rv.end()));

    auto it = rv.rem_range(boost::make_iterator_range_n(rv.begin() + 1, 2));
    BOOST_REQUIRE_EQUAL(it, rv.begin() + 1);
    BOOST_REQUIRE_EQUAL(rv.size(), 3);
    BOOST_CHECK_EQUAL(*(rv.begin()), re1);
    BOOST_CHECK_EQUAL(*(rv.begin() + 1), re4);
    BOOST_CHECK_EQUAL(*(rv.begin() + 2), re5);

    it = rv.rem_range(boost::make_iterator_range_n(rv.begin() + 1, 1));
    BOOST_REQUIRE_EQUAL(rv.size(), 2);
    BOOST_REQUIRE_EQUAL(it, rv.begin() + 1);
    BOOST_CHECK_EQUAL(*(rv.begin()), re1);
    BOOST_CHECK_EQUAL(*(rv.begin() + 1), re5);
}

BOOST_AUTO_TEST_CASE(five_elements_rem_last_two)
{
    const auto re1 = make_range_elem(1024, min_obj_size + 1024, 32_vblocks);
    const auto re2 = make_range_elem(10240, min_obj_size + 2, 20_vblocks);
    const auto re3 = make_range_elem(10240 * 2, min_obj_size + 2, 20_vblocks);
    const auto re4 = make_range_elem(10240 * 3, min_obj_size + 2, 20_vblocks);
    const auto re5 = make_range_elem(10240 * 4, min_obj_size + 2, 20_vblocks);

    range_vector rv;
    BOOST_REQUIRE(rv.add_range(re5).second);
    BOOST_REQUIRE(rv.add_range(re2).second);
    BOOST_REQUIRE(rv.add_range(re3).second);
    BOOST_REQUIRE(rv.add_range(re1).second);
    BOOST_REQUIRE(rv.add_range(re4).second);
    BOOST_CHECK(std::is_sorted(rv.begin(), rv.end()));

    auto it = rv.rem_range(boost::make_iterator_range_n(rv.end() - 2, 2));
    BOOST_REQUIRE_EQUAL(it, rv.end());
    BOOST_REQUIRE_EQUAL(rv.size(), 3);
    BOOST_CHECK_EQUAL(*(rv.begin()), re1);
    BOOST_CHECK_EQUAL(*(rv.begin() + 1), re2);
    BOOST_CHECK_EQUAL(*(rv.begin() + 2), re3);

    it = rv.rem_range(boost::make_iterator_range_n(rv.end() - 2, 2));
    BOOST_REQUIRE_EQUAL(rv.size(), 1);
    BOOST_REQUIRE_EQUAL(it, rv.end());
    BOOST_CHECK_EQUAL(*(rv.begin()), re1);
}

BOOST_AUTO_TEST_CASE(copy_empty_vector)
{
    range_vector rv;
    // The vector must be empty
    BOOST_CHECK(rv.empty());
    BOOST_CHECK(!rv.data());
    BOOST_CHECK_EQUAL(rv.size(), 0);
    BOOST_CHECK_EQUAL(rv.begin(), rv.end());
    BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());
    {
        // Copy construct another vector
        range_vector rv_copy(rv);
        BOOST_CHECK(rv_copy.empty());
        BOOST_CHECK(!rv_copy.data());
        BOOST_CHECK_EQUAL(rv_copy.size(), 0);
        BOOST_CHECK_EQUAL(rv_copy.begin(), rv_copy.end());
        BOOST_CHECK_EQUAL(rv_copy.cbegin(), rv_copy.cend());
    }
    {
        // Copy assign to another vector
        range_vector rv_copy;
        rv_copy = rv;
        BOOST_CHECK(rv_copy.empty());
        BOOST_CHECK(!rv_copy.data());
        BOOST_CHECK_EQUAL(rv_copy.size(), 0);
        BOOST_CHECK_EQUAL(rv_copy.begin(), rv_copy.end());
        BOOST_CHECK_EQUAL(rv_copy.cbegin(), rv_copy.cend());
    }
}

BOOST_AUTO_TEST_CASE(move_empty_vector)
{
    range_vector rv;
    // The vector must be empty
    BOOST_CHECK(rv.empty());
    BOOST_CHECK(!rv.data());
    BOOST_CHECK_EQUAL(rv.size(), 0);
    BOOST_CHECK_EQUAL(rv.begin(), rv.end());
    BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());
    {
        // Move construct another vector
        range_vector rv2(std::move(rv));
        BOOST_CHECK(rv2.empty());
        BOOST_CHECK(!rv2.data());
        BOOST_CHECK_EQUAL(rv2.size(), 0);
        BOOST_CHECK_EQUAL(rv2.begin(), rv2.end());
        BOOST_CHECK_EQUAL(rv2.cbegin(), rv2.cend());
        // The original vector must remain in the same state
        BOOST_CHECK(rv.empty());
        BOOST_CHECK(!rv.data());
        BOOST_CHECK_EQUAL(rv.size(), 0);
        BOOST_CHECK_EQUAL(rv.begin(), rv.end());
        BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());
    }
    {
        // Move assign to another vector
        range_vector rv2;
        rv2 = std::move(rv);
        BOOST_CHECK(rv2.empty());
        BOOST_CHECK(!rv2.data());
        BOOST_CHECK_EQUAL(rv2.size(), 0);
        BOOST_CHECK_EQUAL(rv2.begin(), rv2.end());
        BOOST_CHECK_EQUAL(rv2.cbegin(), rv2.cend());
        // The original vector must remain in the same state
        BOOST_CHECK(rv.empty());
        BOOST_CHECK(!rv.data());
        BOOST_CHECK_EQUAL(rv.size(), 0);
        BOOST_CHECK_EQUAL(rv.begin(), rv.end());
        BOOST_CHECK_EQUAL(rv.cbegin(), rv.cend());
    }
}

BOOST_AUTO_TEST_CASE(copy_non_empty_vector)
{
    const std::array<range_elem, 3> elems = {
        make_range_elem(1024, min_obj_size + 1024, 1024_vblocks),
        make_range_elem(min_obj_size + 2048, min_obj_size + 1024, 2048_vblocks),
        make_range_elem(2 * min_obj_size + 3072, min_obj_size + 1024,
                        3072_vblocks)};

    // Fill the original with some elements
    range_vector rv(elems[1]);
    rv.add_range(elems[0]);
    rv.add_range(elems[2]);
    // Check that it's in the state it should be
    BOOST_CHECK(!rv.empty());
    BOOST_CHECK_EQUAL(rv.size(), elems.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    BOOST_CHECK(
        std::equal(rv.cbegin(), rv.cend(), elems.cbegin(), elems.cend()));
    {
        // Now copy construct from the original and
        // check that both containers are the same.
        range_vector rv2(rv);
        BOOST_CHECK(!rv2.empty());
        BOOST_CHECK_EQUAL(rv2.size(), elems.size());
        BOOST_CHECK(
            std::equal(rv.cbegin(), rv.cend(), rv2.cbegin(), rv2.cend()));
    }
    {
        // Now copy assign from the original and
        // check that both containers are the same.
        range_vector rv2;
        rv2 = rv;
        BOOST_CHECK(!rv2.empty());
        BOOST_CHECK_EQUAL(rv2.size(), elems.size());
        BOOST_CHECK(
            std::equal(rv.cbegin(), rv.cend(), rv2.cbegin(), rv2.cend()));
    }
}

BOOST_AUTO_TEST_CASE(move_non_empty_vector)
{
    const std::array<range_elem, 3> elems = {
        make_range_elem(1024, min_obj_size + 1024, 1024_vblocks),
        make_range_elem(min_obj_size + 2048, min_obj_size + 1024, 2048_vblocks),
        make_range_elem(2 * min_obj_size + 3072, min_obj_size + 1024,
                        3072_vblocks)};

    // Fill the original with some elements
    range_vector rv(elems[1]);
    rv.add_range(elems[0]);
    rv.add_range(elems[2]);
    // Check that it's in the state it should be
    BOOST_CHECK(!rv.empty());
    BOOST_CHECK_EQUAL(rv.size(), elems.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    BOOST_CHECK(
        std::equal(rv.cbegin(), rv.cend(), elems.cbegin(), elems.cend()));
    {
        // Now move construct from a copy of the original and
        // check that both containers are the same.
        range_vector tmp(rv);
        range_vector rv2(std::move(tmp));
        BOOST_CHECK(!rv2.empty());
        BOOST_CHECK_EQUAL(rv2.size(), elems.size());
        BOOST_CHECK(
            std::equal(rv.cbegin(), rv.cend(), rv2.cbegin(), rv2.cend()));
        // Check also that the moved-from vector is now empty
        BOOST_CHECK(tmp.empty());
        BOOST_CHECK(!tmp.data());
        BOOST_CHECK_EQUAL(tmp.size(), 0);
        BOOST_CHECK_EQUAL(tmp.cbegin(), tmp.cend());
    }
    {
        // Now move assign from a copy of the original and
        // check that both containers are the same.
        range_vector tmp(rv);
        range_vector rv2;
        rv2 = std::move(tmp);
        BOOST_CHECK(!rv2.empty());
        BOOST_CHECK_EQUAL(rv2.size(), elems.size());
        BOOST_CHECK(
            std::equal(rv.cbegin(), rv.cend(), rv2.cbegin(), rv2.cend()));
        // Check also that the moved-from vector is now empty
        BOOST_CHECK(tmp.empty());
        BOOST_CHECK(!tmp.data());
        BOOST_CHECK_EQUAL(tmp.size(), 0);
        BOOST_CHECK_EQUAL(tmp.cbegin(), tmp.cend());
    }
}

BOOST_AUTO_TEST_CASE(multiple_elements_add_remove)
{
    // Add multiple non overlapped ranges side by side.
    constexpr bytes64_t rng_offs              = 10240;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 9 * rng_offs, 4 * rng_offs, 7 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 5 * rng_offs, 6 * rng_offs, 8 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_offs, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    {
        // The added ranges must be sorted by the range offset
        auto tmp = values;
        std::sort(tmp.begin(), tmp.end());
        BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
        int i = 0;
        for (auto it = rv.begin(); it != rv.end(); ++it, ++i)
        {
            // The range must be equal to the corresponding range from the
            // sorted range offsets.
            const auto re =
                make_range_elem(tmp[i], rng_offs, bytes2blocks(tmp[i]));
            BOOST_CHECK_EQUAL(*it, re);
        }
    }
    {
        // Remove the elements out of order
        range_vector rv_copy(rv);
        for (auto v : values)
        {
            auto it = std::find_if(rv_copy.cbegin(), rv_copy.cend(), [v](auto r)
                                   {
                                       return v == r.rng_offset();
                                   });
            BOOST_REQUIRE_NE(it, rv_copy.cend());
            if (it != (rv_copy.cend() - 1))
            {
                const auto tmp = *(it + 1);
                // The erase must return the next element
                it = rv_copy.rem_range(it);
                // The small buffer optimization will kick in if we have
                // only one element, and then the returned iterator
                // can't be calculated in advance
                if (rv_copy.size() > 1)
                    BOOST_REQUIRE_EQUAL(*it, tmp);
                else
                    BOOST_CHECK((it == rv_copy.cbegin()) ||
                                (it == rv_copy.cend()));
            }
            else
            {
                it = rv_copy.rem_range(it);
                BOOST_CHECK_EQUAL(it, rv_copy.cend());
            }
        }
    }
    {
        // Remove the elements from the beginning to the end
        range_vector rv_copy(rv);
        while (!rv_copy.empty())
        {
            // The container must always return an iterator to the next
            auto it = rv_copy.rem_range(rv_copy.begin());
            BOOST_CHECK_EQUAL(it, rv_copy.begin());
            if (it != rv_copy.end())
            {
                BOOST_CHECK_EQUAL(*it, *rv_copy.begin());
            }
        }
    }
    {
        // Remove the elements from the end to the beginning
        range_vector rv_copy(rv);
        while (!rv_copy.empty())
        {
            // The container must always return an iterator to the end
            auto it = rv_copy.rem_range(rv_copy.end() - 1);
            BOOST_CHECK_EQUAL(it, rv_copy.end());
        }
    }
}

BOOST_AUTO_TEST_CASE(remove_range_of_ranges)
{
    // Add multiple non overlapped ranges side by side.
    constexpr bytes64_t rng_offs              = 10_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 9 * rng_offs, 4 * rng_offs, 7 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 5 * rng_offs, 6 * rng_offs, 8 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_offs, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    {
        // Remove 3 ranges of ranges out of order
        auto rv_copy(rv);
        constexpr bytes64_t rng_size = 3 * rng_offs;
        // Remove the middle range of ranges.
        auto rng = rv_copy.find_full_range(range{4 * rng_offs, rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 3);
        auto it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.cend() - rng.size());
        BOOST_CHECK_EQUAL(it->rng_offset(), 7 * rng_offs);
        BOOST_CHECK_EQUAL(it->rng_size(), rng_offs);
        BOOST_CHECK_EQUAL(rv_copy.size(), 6);
        // Remove the first range of ranges
        rng = rv_copy.find_full_range(range{1 * rng_offs, rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 3);
        it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.begin());
        BOOST_CHECK_EQUAL(it->rng_offset(), 7 * rng_offs);
        BOOST_CHECK_EQUAL(it->rng_size(), rng_offs);
        BOOST_CHECK_EQUAL(rv_copy.size(), 3);
        // Remove the last range of ranges
        rng = rv_copy.find_full_range(range{7 * rng_offs, rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 3);
        it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.cend());
        BOOST_CHECK_EQUAL(rv_copy.size(), 0);
    }
    {
        // Remove 3 ranges from the beginning to the end
        auto rv_copy(rv);
        constexpr bytes64_t rng_size = 3 * rng_offs;
        // Remove the first range of ranges.
        auto rng = rv_copy.find_full_range(range{1 * rng_offs, rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 3);
        auto it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.begin());
        BOOST_CHECK_EQUAL(it->rng_offset(), 4 * rng_offs);
        BOOST_CHECK_EQUAL(it->rng_size(), rng_offs);
        BOOST_CHECK_EQUAL(rv_copy.size(), 6);
        // Remove the second range of ranges
        rng = rv_copy.find_full_range(range{4 * rng_offs, rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 3);
        it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.begin());
        BOOST_CHECK_EQUAL(it->rng_offset(), 7 * rng_offs);
        BOOST_CHECK_EQUAL(it->rng_size(), rng_offs);
        BOOST_CHECK_EQUAL(rv_copy.size(), 3);
        // Remove the last range of ranges
        rng = rv_copy.find_full_range(range{7 * rng_offs, rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 3);
        it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.begin());
        BOOST_CHECK_EQUAL(rv_copy.size(), 0);
    }
    {
        // Remove 3 ranges from the end to the beginning
        auto rv_copy(rv);
        constexpr bytes64_t rng_size = 3 * rng_offs;
        // Remove the last range of ranges.
        auto rng = rv_copy.find_full_range(range{7 * rng_offs, rng_size});
        BOOST_CHECK_EQUAL(rng.size(), 3);
        // Remove the last range of ranges
        auto it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.cend());
        BOOST_CHECK_EQUAL(rv_copy.size(), 6);
        // Remove the second range of ranges
        rng = rv_copy.find_full_range(range{4 * rng_offs, rng_size});
        BOOST_CHECK_EQUAL(rng.size(), 3);
        // Remove the last range of ranges
        it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.cend());
        BOOST_CHECK_EQUAL(rv_copy.size(), 3);
        // Remove the first range of ranges
        rng = rv_copy.find_full_range(range{1 * rng_offs, rng_size});
        BOOST_CHECK_EQUAL(rng.size(), 3);
        // Remove the last range of ranges
        it = rv_copy.rem_range(rng);
        BOOST_REQUIRE_EQUAL(it, rv_copy.cend());
        BOOST_CHECK_EQUAL(rv_copy.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE(wont_add_overlapped_ranges)
{
    range_vector rv;
    auto add_range_fail = [&](bytes64_t offs, bytes64_t size, auto fail_elem)
    {
        auto re = make_range_elem(
            offs, size,
            volume_blocks64_t::round_up_to_blocks(volume_skip_bytes + offs));
        auto ret = rv.add_range(re);
        BOOST_REQUIRE(!ret.second);
        BOOST_REQUIRE_EQUAL(ret.first, fail_elem);
        return ret.first;
    };
    {
        auto re  = make_range_elem(10_KB, 10_KB, 1024_vblocks);
        auto ret = rv.add_range(re);
        BOOST_REQUIRE(ret.second);
        BOOST_REQUIRE_EQUAL(*ret.first, re);
    }
    // Overlaps with the beginning of the first
    add_range_fail(1, 10_KB, rv.begin());
    // Overlaps with the end of the first
    add_range_fail(20_KB - 1, min_obj_size, rv.begin());
    // Overlaps the whole first element
    add_range_fail(10_KB - 1, 10_KB + 2, rv.begin());
    // Overlaps exactly the first element
    add_range_fail(10_KB, 10_KB, rv.begin());
    {
        // This one doesn't overlap. It must succeed
        auto re  = make_range_elem(30_KB, 10_KB, 2048_vblocks);
        auto ret = rv.add_range(re);
        BOOST_REQUIRE(ret.second);
        BOOST_REQUIRE_EQUAL(*ret.first, re);
    }
    // Overlaps with the beginning of the first
    add_range_fail(1, 10_KB, rv.begin());
    // Overlaps with the end of the first
    add_range_fail(20_KB - 1, min_obj_size, rv.begin());
    // Overlaps the whole first element
    add_range_fail(10_KB - 1, 10_KB + 2, rv.begin());
    // Overlaps exactly the first element
    add_range_fail(10_KB, 10_KB, rv.begin());
    // Overlaps part of the first and part of the second element.
    // Must fail because of the overlap with the first one.
    add_range_fail(20_KB - 1, 10_KB + 2, rv.begin());
    // Overlaps the beginning of the second element
    add_range_fail(20_KB + 1, 10_KB, rv.begin() + 1);
    // Fully overlaps the whole second element
    add_range_fail(30_KB - 1, 10_KB + 2, rv.begin() + 1);
    // Fully overlaps the whole second element
    add_range_fail(30_KB, 10_KB, rv.begin() + 1);
    // Overlap the end of the second element
    add_range_fail(40_KB - 1, 10_KB, rv.begin() + 1);
}

BOOST_AUTO_TEST_CASE(find_full_range_when_no_holes)
{
    // Add multiple non overlapped ranges side by side.
    // There is a hole in-between the two groups of the ranges
    constexpr bytes64_t rng_offs              = 10_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 10 * rng_offs, 4 * rng_offs, 8 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 6 * rng_offs,  7 * rng_offs, 9 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_offs, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));

    {
        // Find the whole first range of ranges.
        const range rng{rng_offs, 4 * rng_offs};
        const auto rngs = rv.find_full_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 4);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin()));
    }
    {
        // Find the whole second range of ranges.
        const range rng{6 * rng_offs, 5 * rng_offs};
        const auto rngs = rv.find_full_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 5);
        BOOST_CHECK(
            std::equal(rngs.begin(), rngs.end(), rv.end() - rngs.size()));
    }
    {
        // Find elements included in the given range, even if they overlap
        // with it. Must return only the first two elements.
        const range rng{rng_offs + 1, rng_offs};
        const auto rngs = rv.find_full_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 2);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin()));
    }
    {
        // Find elements included in the given range, even if they overlap
        // with it. Must return the last two elements.
        const range rng{(10 * rng_offs) - 1, rng_offs};
        const auto rngs = rv.find_full_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 2);
        BOOST_CHECK(
            std::equal(rngs.begin(), rngs.end(), rv.end() - rngs.size()));
    }
    {
        // Find full range when we ask for a part of a single range
        const range rng{(2 * rng_offs) + 1_KB, rng_offs - 2_KB};
        const auto rngs = rv.find_full_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 1);
        BOOST_CHECK_EQUAL(rngs.front(), *(rv.begin() + 1));
    }
}

BOOST_AUTO_TEST_CASE(wont_find_full_range_when_holes)
{
    // Add multiple non overlapped ranges side by side.
    // There is a hole in-between the two groups of the ranges
    constexpr bytes64_t rng_offs              = 10_KB;
    constexpr std::array<bytes64_t, 9> values = {
        2 * rng_offs, 11 * rng_offs, 5 * rng_offs, 10 * rng_offs, 4 * rng_offs,
        3 * rng_offs, 7 * rng_offs,  8 * rng_offs, 12 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_offs, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    {
        // Hole at the beginning. Must not find anything.
        const auto rngs = rv.find_full_range(range{2 * rng_offs - 1, rng_offs});
        BOOST_CHECK(rngs.empty());
    }
    {
        // Hole at the end. Must not find anything.
        const auto rngs =
            rv.find_full_range(range{12 * rng_offs, rng_offs + 1});
        BOOST_CHECK(rngs.empty());
    }
    {
        // Hole at the middle. Must not find anything.
        const auto rngs = rv.find_full_range(range{5 * rng_offs, 3 * rng_offs});
        BOOST_CHECK(rngs.empty());
    }
    {
        // Not present at all. Must not find anything.
        auto rngs = rv.find_full_range(range{rng_offs, rng_offs});
        BOOST_CHECK(rngs.empty());
        rngs = rv.find_full_range(range{13 * rng_offs, rng_offs});
        BOOST_CHECK(rngs.empty());
    }
}

BOOST_AUTO_TEST_CASE(find_full_range_when_single_entry)
{
    range_vector rv;
    const auto ret = rv.add_range(make_range_elem(20_KB, 20_KB, 512_vblocks));
    BOOST_REQUIRE(ret.second);

    auto rngs = rv.find_full_range(range{20_KB - 1, 10_KB});
    BOOST_CHECK(rngs.empty());

    rngs = rv.find_full_range(range{20_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_full_range(range{25_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_full_range(range{30_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_full_range(range{30_KB + 1, 10_KB});
    BOOST_CHECK(rngs.empty());
}

BOOST_AUTO_TEST_CASE(find_exact_range)
{
    // Add multiple non overlapped ranges side by side.
    // There is a hole in-between the two groups of the ranges
    constexpr bytes64_t rng_offs              = 10_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 6 * rng_offs, 4 * rng_offs, 8 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 5 * rng_offs, 7 * rng_offs, 9 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_offs, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));

    // Must find all inserted ranges
    for (size_t i = 0; i < values.size(); ++i)
    {
        const range rng0{values[i], rng_offs};
        auto rngs = rv.find_exact_range(rng0);
        BOOST_REQUIRE(rngs.size() == 1);
        BOOST_CHECK_EQUAL(rngs.front().rng_offset(), rng0.beg());
        BOOST_CHECK_EQUAL(rngs.front().rng_size(), rng0.len());

        // The single range_elem version.
        auto it = rv.find_exact_range(
            make_range_elem(rng0.beg(), rng0.len(), bytes2blocks(rng0.beg())));
        BOOST_REQUIRE_NE(it, rv.cend());
        BOOST_CHECK_EQUAL(it->rng_offset(), rng0.beg());
        BOOST_CHECK_EQUAL(it->rng_size(), rng0.len());

        // Check multiple ranges - all from current position to the end
        for (size_t j = i; j < values.size(); ++j)
        {
            const range rng1{(rv.begin() + i)->rng_offset(),
                             (j - i + 1) * rng_offs};
            rngs = rv.find_exact_range(rng1);
            BOOST_REQUIRE_EQUAL(rngs.size(), (j - i + 1));
            BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin() + i));
        }
    }
    // Must not find inserted ranges if they differ even with 1
    for (size_t i = 0; i < values.size(); ++i)
    {
        auto rngs = rv.find_exact_range(range{values[i] - 1, rng_offs});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i] + 1, rng_offs});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i], rng_offs - 1});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i], rng_offs + 1});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i] + 1, rng_offs + 1});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i] - 1, rng_offs - 1});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i] - 1, rng_offs + 1});
        BOOST_REQUIRE(rngs.empty());
        rngs = rv.find_exact_range(range{values[i] + 1, rng_offs - 1});
        BOOST_REQUIRE(rngs.empty());

        // The single range_elem version.
        auto it = rv.find_exact_range(
            make_range_elem(values[i] - 1, rng_offs, bytes2blocks(values[i])));
        BOOST_REQUIRE_EQUAL(it, rv.cend());
        it = rv.find_exact_range(
            make_range_elem(values[i] + 1, rng_offs, bytes2blocks(values[i])));
        BOOST_REQUIRE_EQUAL(it, rv.cend());
        it = rv.find_exact_range(
            make_range_elem(values[i], rng_offs - 1, bytes2blocks(values[i])));
        BOOST_REQUIRE_EQUAL(it, rv.cend());
        it = rv.find_exact_range(
            make_range_elem(values[i], rng_offs + 1, bytes2blocks(values[i])));
        BOOST_REQUIRE_EQUAL(it, rv.cend());
        it = rv.find_exact_range(make_range_elem(values[i] + 1, rng_offs + 1,
                                                 bytes2blocks(values[i])));
        BOOST_REQUIRE_EQUAL(it, rv.cend());
        it = rv.find_exact_range(make_range_elem(values[i] - 1, rng_offs - 1,
                                                 bytes2blocks(values[i])));
        BOOST_REQUIRE_EQUAL(it, rv.cend());

        // Check multiple ranges - all from current position to the end
        for (size_t j = i; j < values.size(); ++j)
        {
            const auto offs = (rv.begin() + i)->rng_offset();
            const auto size = (j - i + 1) * rng_offs;
            rngs = rv.find_exact_range(range{offs - 1, size});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs + 1, size});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs, size - 1});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs, size + 1});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs - 1, size - 1});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs + 1, size + 1});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs - 1, size + 1});
            BOOST_REQUIRE(rngs.empty());
            rngs = rv.find_exact_range(range{offs + 1, size - 1});
            BOOST_REQUIRE(rngs.empty());
        }
    }
}

BOOST_AUTO_TEST_CASE(find_in_range)
{
    // Add multiple non overlapped ranges side.
    // There are holes between the ranges and a bigger hole in the middle.
    constexpr bytes64_t rng_size              = 10_KB;
    constexpr bytes64_t rng_offs              = 20_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 10 * rng_offs, 4 * rng_offs, 8 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 6 * rng_offs,  7 * rng_offs, 9 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_size, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    {
        // Hole at the beginning. Must find single element.
        const range rng{rng_offs - 1, rng_size};
        const auto rngs = rv.find_in_range(rng);
        BOOST_REQUIRE_EQUAL(rngs.size(), 1);
        BOOST_CHECK_EQUAL(rngs.front().rng_offset(), rng_offs);
        BOOST_CHECK_EQUAL(rngs.front().rng_size(), rng_size);
    }
    {
        // Small hole in between. Must find three elements
        const range rng{rng_offs + 1, 2 * rng_offs};
        const auto rngs = rv.find_in_range(rng);
        BOOST_REQUIRE_EQUAL(rngs.size(), 3);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin()));
    }
    {
        // Big hole in between. Must find three elements
        const range rng{(4 * rng_offs) + 1, 3 * rng_offs};
        const auto rngs = rv.find_in_range(rng);
        BOOST_REQUIRE_EQUAL(rngs.size(), 3);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin() + 3));
    }
    {
        // Hole at the end. Must find single element.
        const range rng{(11 * rng_offs) - rng_size - 1, rng_size};
        const auto rngs = rv.find_in_range(rng);
        BOOST_REQUIRE_EQUAL(rngs.size(), 1);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.end() - 1));
    }
    {
        // Find single range when we ask for a part of it
        const range rng{(3 * rng_offs) + 1_KB, rng_size - 2_KB};
        const auto rngs = rv.find_in_range(rng);
        BOOST_REQUIRE_EQUAL(rngs.size(), 1);
        BOOST_CHECK_EQUAL(rngs.front(), *(rv.begin() + 2));
    }
}

BOOST_AUTO_TEST_CASE(find_in_range_same_as_find_full_range)
{
    // Add multiple non overlapped ranges side by side.
    // There is a hole in-between the two groups of the ranges
    constexpr bytes64_t rng_offs              = 10_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 10 * rng_offs, 4 * rng_offs, 8 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 6 * rng_offs,  7 * rng_offs, 9 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_offs, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));

    {
        // Find the whole first range of ranges.
        const range rng{rng_offs, 4 * rng_offs};
        const auto rngs = rv.find_in_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 4);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin()));
    }
    {
        // Find the whole second range of ranges.
        const range rng{6 * rng_offs, 5 * rng_offs};
        const auto rngs = rv.find_in_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 5);
        BOOST_CHECK(
            std::equal(rngs.begin(), rngs.end(), rv.end() - rngs.size()));
    }
    {
        // Find elements included in the given range, even if they overlap
        // with it. Must return only the first two elements.
        const range rng{rng_offs + 1, rng_offs};
        const auto rngs = rv.find_in_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 2);
        BOOST_CHECK(std::equal(rngs.begin(), rngs.end(), rv.begin()));
    }
    {
        // Find elements included in the given range, even if they overlap
        // with it. Must return the last two elements.
        const range rng{(10 * rng_offs) - 1, rng_offs};
        const auto rngs = rv.find_in_range(rng);
        BOOST_CHECK_EQUAL(rngs.size(), 2);
        BOOST_CHECK(
            std::equal(rngs.begin(), rngs.end(), rv.end() - rngs.size()));
    }
}

BOOST_AUTO_TEST_CASE(wont_find_in_range)
{
    // Add multiple non overlapped ranges side.
    // There are holes between the ranges and a bigger hole in the middle.
    constexpr bytes64_t rng_size              = 10_KB;
    constexpr bytes64_t rng_offs              = 20_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 10 * rng_offs, 4 * rng_offs, 8 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 6 * rng_offs,  7 * rng_offs, 9 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_size, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));
    {
        // Search in the hole at the beginning. Must not find any element.
        auto rngs = rv.find_in_range(range{rng_offs - rng_size, rng_size});
        BOOST_CHECK(rngs.empty());
    }
    {
        // Search in the small hole between the elements.
        // Must not find any element.
        auto rngs = rv.find_in_range(range{2 * rng_offs - rng_size, rng_size});
        BOOST_CHECK(rngs.empty());
    }
    {
        // Search in the big hole between the elements.
        // Must not find any element.
        auto rngs =
            rv.find_in_range(range{4 * rng_offs + rng_size, 3 * rng_size});
        BOOST_CHECK(rngs.empty());
    }
    {
        // Search in the hole at the end. Must not find any element.
        auto rngs = rv.find_in_range(range{10 * rng_offs + rng_size, rng_size});
        BOOST_CHECK(rngs.empty());
    }
}

BOOST_AUTO_TEST_CASE(find_in_range_when_single_entry)
{
    range_vector rv;
    const auto ret = rv.add_range(make_range_elem(20_KB, 20_KB, 512_vblocks));
    BOOST_REQUIRE(ret.second);

    auto rngs = rv.find_in_range(range{10_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 0);

    rngs = rv.find_in_range(range{10_KB + 1, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{20_KB - 1, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{20_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{25_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{30_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{30_KB + 1, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{40_KB - 1, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 1);

    rngs = rv.find_in_range(range{40_KB, 10_KB});
    BOOST_CHECK_EQUAL(rngs.size(), 0);
}

BOOST_AUTO_TEST_CASE(are_continuous)
{
    constexpr bytes64_t rng_size              = 10_KB;
    constexpr bytes64_t rng_offs              = 10_KB;
    constexpr std::array<bytes64_t, 9> values = {
        1 * rng_offs, 10 * rng_offs, 4 * rng_offs, 8 * rng_offs, 3 * rng_offs,
        2 * rng_offs, 6 * rng_offs,  7 * rng_offs, 9 * rng_offs};

    range_vector rv;
    for (auto v : values)
    {
        const auto re  = make_range_elem(v, rng_size, bytes2blocks(v));
        const auto ret = rv.add_range(re);
        BOOST_CHECK(ret.second);
        BOOST_CHECK_EQUAL(*ret.first, re);
    }
    BOOST_CHECK_EQUAL(rv.size(), values.size());
    BOOST_CHECK(std::is_sorted(rv.cbegin(), rv.cend()));

    {
        // Get the first range of ranges. The ranges must be continuous.
        const auto rng = rv.find_full_range(range{1 * rng_offs, 4 * rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 4);
        BOOST_CHECK(range_vector::are_continuous(rng));
    }
    {
        // Get the second range of ranges. The ranges must be continuous.
        const auto rng = rv.find_full_range(range{6 * rng_offs, 5 * rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 5);
        BOOST_CHECK(range_vector::are_continuous(rng));
    }
    {
        // Get a range including parts of the first and second ranges.
        // It must be reported as non continuous.
        const auto rng = rv.find_in_range(range{1 * rng_offs, 10 * rng_size});
        BOOST_REQUIRE_EQUAL(rng.size(), 9);
        BOOST_CHECK(!range_vector::are_continuous(rng));
    }
}

BOOST_AUTO_TEST_CASE(trim_overlaps)
{
    constexpr bytes64_t rng_size   = 100_KB;
    constexpr bytes64_t rng_offs   = 100_KB;
    constexpr bytes64_t small_offs = 40_KB;
    constexpr bytes64_t big_size   = 18 * rng_size;

    range_vector rv;
    rv.add_range(make_range_elem(1 * rng_offs, rng_size, 100_vblocks));
    rv.add_range(make_range_elem(2 * rng_offs, rng_size, 200_vblocks));
    rv.add_range(make_range_elem(4 * rng_offs, rng_size, 400_vblocks));
    rv.add_range(make_range_elem(6 * rng_offs, rng_size, 600_vblocks));
    rv.add_range(make_range_elem(7 * rng_offs, rng_size, 700_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 5);

    {
        // Don't overlap at the beginning and at the end
        const auto rng = rv.trim_overlaps(range{small_offs, big_size});
        BOOST_CHECK_EQUAL(rng.beg(), small_offs);
        BOOST_CHECK_EQUAL(rng.len(), big_size);
    }
    {
        // Overlap at the beginning only. Trimmed with the length
        // of the first two elements
        const bytes64_t offs    = rv.begin()->rng_end_offset() - 1;
        const bytes64_t exp_len = big_size - (rv.begin() + 1)->rng_size() - 1;
        const auto rng = rv.trim_overlaps(range{offs, big_size});
        BOOST_CHECK_EQUAL(rng.beg(), (rv.begin() + 1)->rng_end_offset());
        BOOST_CHECK_EQUAL(rng.len(), exp_len);
    }
    {
        // Overlap at the second only. Trimmed with the length
        // of the overlap with the second element
        const bytes64_t offs    = (rv.begin() + 1)->rng_end_offset() - 5;
        const bytes64_t exp_len = big_size - 5;
        const auto rng = rv.trim_overlaps(range{offs, big_size});
        BOOST_CHECK_EQUAL(rng.beg(), (rv.begin() + 1)->rng_end_offset());
        BOOST_CHECK_EQUAL(rng.len(), exp_len);
    }
    {
        // Overlaps with ranges but not at the ends.
        // Nothing will be trimmed.
        const bytes64_t offs    = (rv.begin() + 1)->rng_end_offset();
        const bytes64_t exp_len = big_size;
        const auto rng = rv.trim_overlaps(range{offs, big_size});
        BOOST_CHECK_EQUAL(rng.beg(), offs);
        BOOST_CHECK_EQUAL(rng.len(), exp_len);
    }
    {
        // Overlaps with ranges but not at the ends.
        // Nothing will be trimmed.
        const bytes64_t offs    = (rv.begin() + 2)->rng_offset() - 1;
        const bytes64_t exp_len = big_size;
        const auto rng = rv.trim_overlaps(range{offs, big_size});
        BOOST_CHECK_EQUAL(rng.beg(), offs);
        BOOST_CHECK_EQUAL(rng.len(), exp_len);
    }
    {
        // Overlaps at the beginning with the third element.
        // The beginning will be trimmed.
        const bytes64_t offs    = (rv.begin() + 2)->rng_offset();
        const bytes64_t exp_len = big_size - rng_size;
        const auto rng = rv.trim_overlaps(range{offs, big_size});
        BOOST_CHECK_EQUAL(rng.beg(), (rv.begin() + 2)->rng_end_offset());
        BOOST_CHECK_EQUAL(rng.len(), exp_len);
    }
    {
        // Overlaps at the end with the last element.
        // The end will be trimmed with the size of two elements
        const bytes64_t offs = small_offs;
        const bytes64_t len  = (rv.begin() + 4)->rng_end_offset() - small_offs;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK_EQUAL(rng.beg(), small_offs);
        BOOST_CHECK_EQUAL(rng.len(), len - 2 * rng_size);
    }
    {
        // The range finishes in the gap between the 3rd and the 4th elements.
        // Nothing should be trimmed.
        const bytes64_t offs = small_offs;
        const bytes64_t len  = (rv.begin() + 3)->rng_offset() - small_offs;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK_EQUAL(rng.beg(), small_offs);
        BOOST_CHECK_EQUAL(rng.len(), len);
    }
    {
        // The range finishes into the middle element.
        // It should be trimmed at the end with the overlap size.
        const bytes64_t offs = small_offs;
        const bytes64_t len  = (rv.begin() + 2)->rng_end_offset() - small_offs;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK_EQUAL(rng.beg(), small_offs);
        BOOST_CHECK_EQUAL(rng.len(), len - rng_size);
    }
    {
        // The range overlaps at the beginning and at the end with the
        // range elements. Should be trimmed at both ends.
        const bytes64_t offs = rv.begin()->rng_end_offset() - 1;
        const bytes64_t len  = (rv.begin() + 4)->rng_offset() + 1 - offs;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK_EQUAL(rng.beg(), (rv.begin() + 1)->rng_end_offset());
        BOOST_CHECK_EQUAL(rng.len(), len - (2 * rng_size) - 2);
    }
    {
        // The range overlaps exactly the first and the second elements.
        // The whole must be trimmed.
        const bytes64_t offs = rv.begin()->rng_offset();
        const bytes64_t len  = rng_size;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK(rng.empty());
    }
    {
        // The range lays inside the middle element.
        // The whole must be trimmed.
        const bytes64_t offs = (rv.begin() + 2)->rng_offset() + 1;
        const bytes64_t len  = rng_size - 2;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK(rng.empty());
    }
    {
        // The range lays inside the last two elements.
        // The whole must be trimmed.
        const bytes64_t offs = (rv.begin() + 3)->rng_offset() + 1;
        const bytes64_t len  = (2 * rng_size) - 1;
        const auto rng = rv.trim_overlaps(range{offs, len});
        BOOST_CHECK(rng.empty());
    }
}

BOOST_AUTO_TEST_CASE(trim_overlaps_single_range_elem_at_beg)
{
    constexpr bytes64_t rng_offs = 100_KB;
    constexpr bytes64_t rng_size = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 1);

    const auto rng = rv.trim_overlaps(range{rng_offs, rng_size + 10_KB});
    BOOST_CHECK_EQUAL(rng, (range{200_KB, 10_KB}));
}

BOOST_AUTO_TEST_CASE(trim_overlaps_two_range_elem_at_beg)
{
    constexpr bytes64_t rng_offs = 100_KB;
    constexpr bytes64_t rng_size = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    rv.add_range(make_range_elem(rng_offs + rng_size, rng_size, 500_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 2);

    const auto rng = rv.trim_overlaps(range{rng_offs, 220_KB});
    BOOST_CHECK_EQUAL(rng, (range{300_KB, 20_KB}));
}

BOOST_AUTO_TEST_CASE(trim_overlaps_single_range_elem_at_end)
{
    constexpr bytes64_t rng_offs = 100_KB;
    constexpr bytes64_t rng_size = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 1);

    const auto rng =
        rv.trim_overlaps(range{rng_offs - 10_KB, rng_size + 10_KB});
    BOOST_CHECK_EQUAL(rng, (range{rng_offs - 10_KB, 10_KB}));
}

BOOST_AUTO_TEST_CASE(trim_overlaps_single_range_elem_at_end2)
{
    constexpr bytes64_t rng_offs = 100_KB;
    constexpr bytes64_t rng_size = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 1);

    const auto rng =
        rv.trim_overlaps(range{rng_offs - 10_KB, rng_size + 20_KB});
    BOOST_CHECK_EQUAL(rng, (range{rng_offs - 10_KB, rng_size + 20_KB}));
}

BOOST_AUTO_TEST_CASE(trim_overlaps_two_range_elems_at_end)
{
    constexpr bytes64_t rng_offs = 100_KB;
    constexpr bytes64_t rng_size = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    rv.add_range(make_range_elem(rng_offs + rng_size, rng_size, 500_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 2);

    auto rng = rv.trim_overlaps(range{rng_offs - 10_KB, 2 * rng_size + 10_KB});
    BOOST_CHECK_EQUAL(rng, (range{rng_offs - 10_KB, 10_KB}));

    rng = rv.trim_overlaps(range{rng_offs - 10_KB, rng_size + 10_KB});
    BOOST_CHECK_EQUAL(rng, (range{rng_offs - 10_KB, 10_KB}));
}

BOOST_AUTO_TEST_CASE(trim_overlaps_single_range_elem_overlap_all)
{
    constexpr bytes64_t rng_size = 100_KB;
    constexpr bytes64_t rng_offs = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 1);

    const auto rng = rv.trim_overlaps(range{rng_offs, 10_KB});
    BOOST_CHECK_EQUAL(rng, range{});
}

BOOST_AUTO_TEST_CASE(trim_overlaps_single_range_elem_overlap_all_exact)
{
    constexpr bytes64_t rng_size = 100_KB;
    constexpr bytes64_t rng_offs = 100_KB;

    range_vector rv;
    rv.add_range(make_range_elem(rng_offs, rng_size, 100_vblocks));
    rv.add_range(make_range_elem(rng_offs + rng_size, rng_size, 500_vblocks));
    BOOST_REQUIRE_EQUAL(rv.size(), 2);

    const auto rng = rv.trim_overlaps(range{rng_offs, rng_size * 2});
    BOOST_CHECK_EQUAL(rng, range{});
}

BOOST_AUTO_TEST_CASE(save_load_success_single_elem)
{
    std::array<uint8_t, 1_KB> buf;

    range_vector rv1(make_range_elem(1_KB, min_obj_size, 32_vblocks));
    rv_elem_set_in_memory(rv1.begin(), true);
    rv_elem_atomic_inc_readers(rv1.begin());
    {
        memory_writer wr(buf.data(), buf.size());
        rv1.save(wr);
    }

    range_vector rv2;
    {
        memory_reader rd(buf.data(), buf.size());
        rv2.load(rd);
    }

    BOOST_REQUIRE_EQUAL(rv1.size(), rv2.size());
    BOOST_CHECK(std::equal(rv1.begin(), rv1.end(), rv2.begin()));
    BOOST_CHECK_EQUAL(rv1.begin()->cnt_readers(), rv2.begin()->cnt_readers());
    BOOST_CHECK_EQUAL(rv1.begin()->in_memory(), rv2.begin()->in_memory());
}

BOOST_AUTO_TEST_CASE(save_load_success_multi_elems)
{
    std::array<uint8_t, 1_KB> buf;

    range_vector rv1;
    rv1.add_range(make_range_elem(1_KB, min_obj_size, 32_vblocks));
    rv1.add_range(make_range_elem(10_KB, min_obj_size, 64_vblocks));
    rv1.add_range(make_range_elem(20_KB, min_obj_size, 96_vblocks));
    BOOST_REQUIRE_EQUAL(rv1.size(), 3);
    rv_elem_set_in_memory(rv1.begin() + 1, true);
    rv_elem_atomic_inc_readers(rv1.begin() + 1);
    {
        memory_writer wr(buf.data(), buf.size());
        rv1.save(wr);
    }

    range_vector rv2;
    {
        memory_reader rd(buf.data(), buf.size());
        rv2.load(rd);
    }

    BOOST_REQUIRE_EQUAL(rv1.size(), rv2.size());
    BOOST_CHECK(std::equal(rv1.begin(), rv1.end(), rv2.begin()));
    BOOST_CHECK_EQUAL((rv1.begin() + 1)->cnt_readers(),
                      (rv2.begin() + 1)->cnt_readers());
    BOOST_CHECK_EQUAL((rv1.begin() + 1)->in_memory(),
                      (rv2.begin() + 1)->in_memory());
}

BOOST_AUTO_TEST_CASE(save_load_fail_invalid_ranges_count)
{
    std::array<uint8_t, 1_KB> buf;

    range_vector rv1;
    rv1.add_range(make_range_elem(1_KB, min_obj_size, 32_vblocks));
    rv1.add_range(make_range_elem(10_KB, min_obj_size, 64_vblocks));
    rv1.add_range(make_range_elem(20_KB, min_obj_size, 96_vblocks));
    BOOST_REQUIRE_EQUAL(rv1.size(), 3);
    {
        memory_writer wr(buf.data(), buf.size());
        rv1.save(wr);
    }

    // This is not the usual unit test because we use details of
    // range_vector implementation, but still ...
    // Corrupt the range_vector data, after the magic 4 bytes.
    buf[2 * sizeof(uint32_t) - 1] = 0XFF;
    {
        range_vector rv2;
        memory_reader rd(buf.data(), buf.size());
        BOOST_CHECK(!rv2.load(rd));
        BOOST_CHECK(rv2.empty());
    }
}

BOOST_AUTO_TEST_CASE(save_load_fail_no_data_no_range_elem)
{
    std::array<uint8_t, 1_KB> buf;

    range_vector rv1(make_range_elem(1_KB, min_obj_size, 32_vblocks));
    BOOST_REQUIRE_EQUAL(rv1.size(), 1);
    {
        memory_writer wr(buf.data(), buf.size());
        rv1.save(wr);
    }

    // This is not the usual unit test because we use details of
    // range_vector implementation, but still ...
    // Corrupt the range_vector magic bytes.
    buf[0] = 0XFF;
    {
        range_vector rv2;
        memory_reader rd(buf.data(), buf.size());
        BOOST_CHECK(!rv2.load(rd));
        BOOST_CHECK(rv2.empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
