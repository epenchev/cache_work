#include <boost/test/unit_test.hpp>

#include "../mem_fn_delegate.h"

using namespace x3me::utils;

BOOST_AUTO_TEST_SUITE(tests_mem_fn_delegate)

BOOST_AUTO_TEST_CASE(test_create)
{
    struct arg
    {
    };

    using mfd = mem_fn_delegate<int(const arg&, float, int, double)>;

    struct test
    {
        int func(const arg&, float f, int i, double d)
        {
            return static_cast<int>(f + i + d);
        }
    };

    test t;
    auto d = mfd::create<test, &test::func>(&t);

    int i           = 5;
    const double dd = 3.3;
    BOOST_CHECK(d(arg{}, 2.2f, i, dd) == 10);
}

BOOST_AUTO_TEST_CASE(test_assign)
{
    struct arg
    {
        int k;
    };

    using mfd = mem_fn_delegate<int(const arg&, arg)>;

    struct test
    {
        int func(const arg& a1, arg a2) { return a1.k + a2.k; }
    };

    test t;
    mfd d;
    d.assign<test, &test::func>(&t);

    BOOST_CHECK(d(arg{5}, arg{6}) == 11);
}

BOOST_AUTO_TEST_CASE(test_operator_bool)
{
    struct arg
    {
        int k;
    };

    using mfd = mem_fn_delegate<int(const arg&, arg)>;

    struct test
    {
        int func(const arg& a1, arg a2) { return a1.k + a2.k; }
    };

    test t;
    mfd d;
    BOOST_CHECK(!d);

    d.assign<test, &test::func>(&t);

    BOOST_CHECK(d);
}

namespace
{
struct test_move
{
    static int count_moves_;

    test_move() = default;
    test_move(test_move&&) { ++count_moves_; }
};
int test_move::count_moves_ = 0;
}

BOOST_AUTO_TEST_CASE(test_argument_passing)
{
    using mfd = mem_fn_delegate<int(const double&, const double*, float, int&,
                                    test_move&&)>;

    struct test
    {
        int func(const double& arg, const double* parg, float f, int& i,
                 test_move&& v)
        {
            test_move vv(std::move(v));
            BOOST_CHECK(&arg == parg); // arg is not copied
            BOOST_CHECK(vv.count_moves_ == 1);
            int ii = i;
            i = 100;
            return static_cast<int>(f + ii);
        }
    };

    test t;
    auto d = mfd::create<test, &test::func>(&t);

    int i           = 5;
    const double dd = 3.3;
    test_move v;
    d(dd, &dd, 2.2f, i, std::move(v));
    BOOST_CHECK(i == 100);
    BOOST_CHECK(test_move::count_moves_ == 1);
}

BOOST_AUTO_TEST_SUITE_END()
