#include <vector>

#include <boost/test/unit_test.hpp>

#include "../rcu_resource.h"

using namespace x3me::thread;

using int_arr_t = std::vector<int>;
using rcu_arr_t = rcu_resource<int_arr_t>;

////////////////////////////////////////////////////////////////////////////////
// The actual thread safety of the rcu_resource functionality is not tested
// in these unit tests. I couldn't figure out a way to reliably test it with
// unit tests.

BOOST_AUTO_TEST_SUITE(tests_rcu_resource)

BOOST_AUTO_TEST_CASE(test_typedefs)
{
    BOOST_CHECK((std::is_same<rcu_arr_t::type, const int_arr_t>::value));
    BOOST_CHECK((std::is_same<rcu_arr_t::resource_type,
                              std::shared_ptr<const int_arr_t>>::value));
}

BOOST_AUTO_TEST_CASE(test_default_construction)
{
    rcu_arr_t arr;
}

BOOST_AUTO_TEST_CASE(test_in_place_construction)
{
    auto t = {1, 2, 3};
    rcu_arr_t arr(x3me::thread::in_place, t);
    auto r = arr.read_copy();
    BOOST_CHECK_EQUAL(r->at(0), 1);
    BOOST_CHECK_EQUAL(r->at(1), 2);
    BOOST_CHECK_EQUAL(r->at(2), 3);
}

BOOST_AUTO_TEST_CASE(test_copy_construction)
{
    auto t = {1, 2, 3};
    rcu_arr_t arr(x3me::thread::in_place, t);
    rcu_arr_t arr2(arr);
    auto r  = arr.read_copy();
    auto r2 = arr2.read_copy();
    BOOST_CHECK_EQUAL(r->at(0), r2->at(0));
    BOOST_CHECK_EQUAL(r->at(1), r2->at(1));
    BOOST_CHECK_EQUAL(r->at(2), r2->at(2));
}

BOOST_AUTO_TEST_CASE(test_move_construction)
{
    auto t = {1, 2, 3};
    rcu_arr_t arr(x3me::thread::in_place, t);
    rcu_arr_t arr2(std::move(arr));
    auto r  = arr.read_copy();
    auto r2 = arr2.read_copy();
    BOOST_CHECK(!r);
    BOOST_CHECK_EQUAL(r2->at(0), 1);
    BOOST_CHECK_EQUAL(r2->at(1), 2);
    BOOST_CHECK_EQUAL(r2->at(2), 3);
}

BOOST_AUTO_TEST_CASE(test_copy_assignment)
{
    auto t = {1, 2, 3};
    rcu_arr_t arr(x3me::thread::in_place, t);
    rcu_arr_t arr2;
    arr2    = arr;
    auto r  = arr.read_copy();
    auto r2 = arr2.read_copy();
    BOOST_CHECK_EQUAL(r->at(0), r2->at(0));
    BOOST_CHECK_EQUAL(r->at(1), r2->at(1));
    BOOST_CHECK_EQUAL(r->at(2), r2->at(2));
}

BOOST_AUTO_TEST_CASE(test_move_assignment)
{
    auto t = {1, 2, 3};
    rcu_arr_t arr(x3me::thread::in_place, t);
    rcu_arr_t arr2;
    arr2    = std::move(arr);
    auto r  = arr.read_copy();
    auto r2 = arr2.read_copy();
    BOOST_CHECK(!r);
    BOOST_CHECK_EQUAL(r2->at(0), 1);
    BOOST_CHECK_EQUAL(r2->at(1), 2);
    BOOST_CHECK_EQUAL(r2->at(2), 3);
}

BOOST_AUTO_TEST_CASE(test_release)
{
    auto t = {1, 2, 3};
    rcu_arr_t arr(x3me::thread::in_place, t);
    auto r = arr.release();
    BOOST_CHECK_EQUAL(r->at(0), 1);
    BOOST_CHECK_EQUAL(r->at(1), 2);
    BOOST_CHECK_EQUAL(r->at(2), 3);
    r = arr.read_copy();
    BOOST_CHECK(!r);
}

BOOST_AUTO_TEST_CASE(test_update)
{
    auto t  = {1, 2, 3};
    auto t2 = {4, 5, 6};
    rcu_arr_t arr(x3me::thread::in_place, t);
    rcu_arr_t arr2(x3me::thread::in_place, t2);
    arr.update(arr2.release());
    auto r  = arr.read_copy();
    auto r2 = arr2.read_copy();
    BOOST_CHECK(!r2);
    BOOST_CHECK_EQUAL(r->at(0), 4);
    BOOST_CHECK_EQUAL(r->at(1), 5);
    BOOST_CHECK_EQUAL(r->at(2), 6);
}

BOOST_AUTO_TEST_SUITE_END()
