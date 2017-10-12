#include <array>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "../array_view.h"
#include "../utils.h"

using namespace x3me;
using namespace x3me::mem_utils;

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(tests_array_view)

BOOST_AUTO_TEST_CASE(test_default_construct)
{
    array_view<int> av;
    BOOST_CHECK(!av);
    BOOST_CHECK(av.empty());
    BOOST_CHECK(av.data() == nullptr);
    BOOST_CHECK_EQUAL(av.size(), 0);
}

BOOST_AUTO_TEST_CASE(test_construct_data_size)
{
    {
        int arr[] = {1, 2, 3, 4};
        auto av = make_array_view(utils::data(arr), utils::size(arr));
        BOOST_CHECK(av);
        BOOST_CHECK(av.data() == utils::data(arr));
        BOOST_CHECK_EQUAL(av.size(), utils::size(arr));
        BOOST_CHECK((std::is_same<decltype(av)::value_type, int>::value));
    }
    {
        constexpr auto av = make_array_view("abcd", 4);
        BOOST_CHECK(av);
        BOOST_CHECK(memcmp(av.data(), "abcd", 4) == 0);
        BOOST_CHECK_EQUAL(av.size(), 4);
        BOOST_CHECK(
            (std::is_same<decltype(av)::value_type, const char>::value));
    }
}

BOOST_AUTO_TEST_CASE(test_construct_from_container)
{
    {
        std::vector<int> arr = {1, 2, 3, 4};
        auto av = make_array_view(arr);
        BOOST_CHECK(av);
        BOOST_CHECK(av.data() == utils::data(arr));
        BOOST_CHECK_EQUAL(av.size(), utils::size(arr));
        BOOST_CHECK((std::is_same<decltype(av)::value_type, int>::value));
    }
    {
        const std::vector<int> arr = {1, 2, 3, 4};
        auto av = make_array_view(arr);
        BOOST_CHECK(av);
        BOOST_CHECK(av.data() == utils::data(arr));
        BOOST_CHECK_EQUAL(av.size(), utils::size(arr));
        BOOST_CHECK((std::is_same<decltype(av)::value_type, const int>::value));
    }
}

BOOST_AUTO_TEST_CASE(test_implicit_constructor)
{
    auto fn = [](array_view<int> av, const std::array<int, 4>& arr)
    {
        BOOST_CHECK(av);
        BOOST_CHECK(av.data() == utils::data(arr));
        BOOST_CHECK_EQUAL(av.size(), utils::size(arr));
        BOOST_CHECK((std::is_same<decltype(av)::value_type, int>::value));
    };
    std::array<int, 4> arr = {1, 2, 3, 4};
    fn(arr, arr);
}

BOOST_AUTO_TEST_CASE(test_elements_access)
{
    auto av = make_array_view("ABCD", 4);
    BOOST_CHECK_EQUAL(av.front(), "ABCD"[0]);
    for (auto i = 0U; i < 4; ++i)
    {
        BOOST_CHECK_EQUAL(av[i], "ABCD"[i]);
    }
    BOOST_CHECK_EQUAL(av.back(), "ABCD"[3]);
}

BOOST_AUTO_TEST_CASE(test_iterator_access)
{
    {
        auto av = make_array_view("ABCD", 4);
        BOOST_CHECK(
            std::equal(std::begin(av), std::end(av), std::begin("ABCD")));
        BOOST_CHECK(std::equal(av.rbegin(), av.rend(), std::begin("DCBA")));
    }
    {
        const auto av = make_array_view("ABCD", 4);
        BOOST_CHECK(
            std::equal(std::begin(av), std::end(av), std::begin("ABCD")));
        BOOST_CHECK(std::equal(av.crbegin(), av.crend(), std::begin("DCBA")));
    }
}

BOOST_AUTO_TEST_SUITE_END()
