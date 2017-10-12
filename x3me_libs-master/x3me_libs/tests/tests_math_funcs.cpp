
#include <boost/test/unit_test.hpp>

#include "../math_funcs.h"

using namespace x3me::math;

BOOST_AUTO_TEST_SUITE(tests_math)

BOOST_AUTO_TEST_CASE(divide_round_up_correctness)
{
    BOOST_CHECK_EQUAL(divide_round_up(long(88), short(22)), 4);
    BOOST_CHECK_EQUAL(divide_round_up(unsigned(100), (unsigned short)(11)), 10);
}

BOOST_AUTO_TEST_CASE(ranges_overlap_no_overlap)
{
    int x1 = 2, x2 = 5;
    long y1 = 5, y2 = 8;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), 0);
    y1 = 8, y2 = 88;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), 0);
}

BOOST_AUTO_TEST_CASE(ranges_overlap_front)
{
    short x1 = 20, x2 = 40;
    char y1 = 30, y2 = 80;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), (x2 - y1));
    y1 = 10, y2 = 22;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), (y2 - x1));
}

BOOST_AUTO_TEST_CASE(ranges_overlap_back)
{
    unsigned x1 = 200333, x2 = 200666;
    unsigned long long y1 = 200000, y2 = 200400;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), (y2 - x1));
    x1 = 10, x2 = 200100;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), (x2 - y1));
}

BOOST_AUTO_TEST_CASE(ranges_overlap_contain)
{
    size_t x1 = 200, x2 = 400;
    unsigned y1 = 250, y2 = 350;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), (y2 - y1));
    y1 = 100, y2 = 500;
    BOOST_CHECK_EQUAL(ranges_overlap(x1, x2, y1, y2), (x2 - x1));
}

BOOST_AUTO_TEST_SUITE_END()
