#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>

#include <boost/algorithm/hex.hpp>
#include <boost/test/unit_test.hpp>

#include "common.h"

#include "../infohash.h"

using namespace x3me::bt_utils;

BOOST_AUTO_TEST_SUITE(tests_infohash)

BOOST_AUTO_TEST_CASE(test_construction)
{
    auto string_ih = random_ih();
    BOOST_CHECK_EQUAL(string_ih.size(), infohash_size);

    infohash ih(string_ih.data());

    BOOST_CHECK_EQUAL((std::equal(ih.cbegin(), ih.cend(), string_ih.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_copy_construction)
{
    auto string_ih = random_ih();

    infohash ih1(string_ih.data());
    infohash ih2(ih1);

    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_copy_construction_frow_view)
{
    auto string_ih = random_ih();

    infohash_view ih1(string_ih.data());
    infohash ih2(ih1);

    BOOST_CHECK_NE((void*)ih1.data(), (void*)ih2.data());
    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_copy_assignement)
{
    auto string_ih = random_ih();

    infohash ih1(string_ih.data());
    infohash ih2;

    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      false);

    ih2 = ih1;
    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_copy_assignement_from_view)
{
    auto string_ih = random_ih();

    infohash_view ih1(string_ih.data());
    infohash ih2;

    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      false);
    ih2 = ih1;
    BOOST_CHECK_NE((void*)ih1.data(), (void*)ih2.data());
    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_move_construction)
{
    auto string_ih = random_ih();

    infohash ih1(string_ih.data());
    infohash ih2(std::move(ih1));

    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_move_assignement)
{
    auto string_ih = random_ih();

    infohash ih1(string_ih.data());
    infohash ih2;

    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      false);

    ih2 = std::move(ih1);
    BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), ih2.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_assign)
{
    auto string_ih = random_ih();

    infohash ih;

    BOOST_CHECK_EQUAL((std::equal(ih.cbegin(), ih.cend(), string_ih.cbegin())),
                      false);

    ih.assign(string_ih.data());

    BOOST_CHECK_EQUAL((std::equal(ih.cbegin(), ih.cend(), string_ih.cbegin())),
                      true);
}

BOOST_AUTO_TEST_CASE(test_check_equal_infohash_infohash_view)
{
    auto string_ih = random_ih();
    infohash ih(string_ih.data());

    infohash_view ihv1(string_ih.data());
    BOOST_CHECK_EQUAL(ih, ihv1);
    BOOST_CHECK_EQUAL(ihv1, ih);

    infohash_view ihv2(ih.data());
    BOOST_CHECK_EQUAL(ih, ihv2);
    BOOST_CHECK_EQUAL(ihv2, ih);

    auto sih2 = random_ih();
    infohash_view ihv3(sih2.data());
    BOOST_CHECK(ih != ihv3);
    BOOST_CHECK(ihv3 != ih);
}

BOOST_AUTO_TEST_CASE(test_non_member_swap)
{
    auto string_ih1 = random_ih();
    auto string_ih2 = random_ih();

    BOOST_CHECK_EQUAL((std::equal(string_ih1.cbegin(), string_ih1.cend(),
                                  string_ih2.cbegin())),
                      false);

    infohash ih1(string_ih1.data());
    infohash ih2(string_ih2.data());

    BOOST_CHECK_EQUAL(
        (std::equal(ih1.cbegin(), ih1.cend(), string_ih1.cbegin())), true);
    BOOST_CHECK_EQUAL(
        (std::equal(ih2.cbegin(), ih2.cend(), string_ih2.cbegin())), true);

    using std::swap;
    swap(ih1, ih2);

    BOOST_CHECK_EQUAL(
        (std::equal(ih1.cbegin(), ih1.cend(), string_ih2.cbegin())), true);
    BOOST_CHECK_EQUAL(
        (std::equal(ih2.cbegin(), ih2.cend(), string_ih1.cbegin())), true);
}

BOOST_AUTO_TEST_CASE(test_member_swap)
{
    auto string_ih1 = random_ih();
    auto string_ih2 = random_ih();

    BOOST_CHECK_EQUAL((std::equal(string_ih1.cbegin(), string_ih1.cend(),
                                  string_ih2.cbegin())),
                      false);

    infohash ih1(string_ih1.data());
    infohash ih2(string_ih2.data());

    BOOST_CHECK_EQUAL(
        (std::equal(ih1.cbegin(), ih1.cend(), string_ih1.cbegin())), true);
    BOOST_CHECK_EQUAL(
        (std::equal(ih2.cbegin(), ih2.cend(), string_ih2.cbegin())), true);

    ih1.swap(ih2);

    BOOST_CHECK_EQUAL(
        (std::equal(ih1.cbegin(), ih1.cend(), string_ih2.cbegin())), true);
    BOOST_CHECK_EQUAL(
        (std::equal(ih2.cbegin(), ih2.cend(), string_ih1.cbegin())), true);
}

BOOST_AUTO_TEST_CASE(test_comparisson)
{
    for (int i = 0; i < 50000; ++i)
    {
        auto string_ih1 = random_ih();
        auto string_ih2 = random_ih();

        BOOST_CHECK_EQUAL((std::equal(string_ih1.cbegin(), string_ih1.cend(),
                                      string_ih2.cbegin())),
                          false);

        infohash ih1(string_ih1.data());
        infohash ih2(string_ih2.data());

        BOOST_CHECK_EQUAL(
            (std::equal(ih1.cbegin(), ih1.cend(), string_ih1.cbegin())), true);
        BOOST_CHECK_EQUAL(
            (std::equal(ih2.cbegin(), ih2.cend(), string_ih2.cbegin())), true);

        BOOST_CHECK_EQUAL((std::less<infohash>()(ih1, ih2)),
                          (std::less<std::string>()(string_ih1, string_ih2)));
        BOOST_CHECK_EQUAL(
            (std::less_equal<infohash>()(ih1, ih2)),
            (std::less_equal<std::string>()(string_ih1, string_ih2)));
        BOOST_CHECK_EQUAL(
            (std::greater<infohash>()(ih1, ih2)),
            (std::greater<std::string>()(string_ih1, string_ih2)));
        BOOST_CHECK_EQUAL(
            (std::greater_equal<infohash>()(ih1, ih2)),
            (std::greater_equal<std::string>()(string_ih1, string_ih2)));
        BOOST_CHECK_EQUAL(
            (std::equal_to<infohash>()(ih1, ih2)),
            (std::equal_to<std::string>()(string_ih1, string_ih2)));
        BOOST_CHECK_EQUAL(
            (std::not_equal_to<infohash>()(ih1, ih2)),
            (std::not_equal_to<std::string>()(string_ih1, string_ih2)));
    }
}

BOOST_AUTO_TEST_CASE(test_streaming)
{
    for (int i = 0; i < 50000; ++i)
    {
        auto string_ih = random_ih();
        infohash ih(string_ih.data());

        std::ostringstream oss;
        oss << ih;

        std::string hex_sih;
        boost::algorithm::hex(string_ih.cbegin(), string_ih.cend(),
                              std::back_inserter(hex_sih));

        auto hex_ih = oss.str();
        BOOST_CHECK_EQUAL(hex_ih.size(), infohash_size * 2);
        BOOST_CHECK_EQUAL(oss.str(), hex_sih);
    }
}

BOOST_AUTO_TEST_CASE(test_hashing_stupid_way)
{
    enum
    {
        count = 50000
    };
    std::vector<std::string> sih_array;
    sih_array.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        sih_array.emplace_back(random_ih());
    }
    std::unordered_set<infohash> ih_set;
    for (const auto& sih : sih_array)
    {
        ih_set.emplace(sih.data());
    }

    BOOST_CHECK_EQUAL(ih_set.size(), sih_array.size());

    for (const auto& sih : sih_array)
    {
        auto found = ih_set.find(infohash(sih.data()));
        BOOST_CHECK_EQUAL((found != ih_set.cend()), true);
    }
}

BOOST_AUTO_TEST_SUITE_END()
