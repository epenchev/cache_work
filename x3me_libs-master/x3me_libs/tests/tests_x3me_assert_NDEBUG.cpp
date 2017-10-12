#define NDEBUG

#include <boost/test/unit_test.hpp>

#define X3ME_ASSERT_DO_THROW // We can't unit tests asserts
#include "../x3me_assert.h"

BOOST_AUTO_TEST_SUITE(tests_x3me_assert_NDEBUG)

BOOST_AUTO_TEST_CASE(test_x3me_enforce)
{
    // Just few asserts to check the proper compilation
    auto test_fn = []
    {
        return true;
    };
    auto test_fn2 = [](int, const char*)
    {
        return true;
    };
    X3ME_ENFORCE(test_fn());
    X3ME_ENFORCE(test_fn(), "With msg");
    X3ME_ENFORCE(test_fn2(42, "Test"));
    X3ME_ENFORCE(test_fn2(42, "Test"), "With msg");
    int k  = 43;
    bool f = true;
    X3ME_ENFORCE((k == 43) && (f == true));
    X3ME_ENFORCE((k == 43) && (f == true), "With msg");
    // Uncomment to see the compilation error in case of too many arguments
    // X3ME_ENFORCE((k == 43), (f == true), (k == 43), (f == true), "With msg");

    // The presence/absence of the 'message' in the throw exception
    // verifies that the correct enforce has been triggered.
    auto fn_msg = []
    {
        volatile int i = 42;
        X3ME_ENFORCE(i == 42); // This won't throw
        X3ME_ENFORCE(i != 42, "The 'i' must be 42");
    };
    auto fn_no_msg = []
    {
        volatile int i = 42;
        X3ME_ENFORCE(i == 42, "This won't throw"); // This won't throw
        X3ME_ENFORCE(i != 42);
    };

    // We don't use BOOST_CHECK_THROW because we want to check some
    // properties of the thrown exceptions.
    // The X3ME_ENFORCE must work even when the NDEBUG is defined.
    bool ex_thrown = false;
    try
    {
        fn_msg();
    }
    catch (const x3me::assert::assert_fail& ex)
    {
        ex_thrown = true;
        BOOST_CHECK(ex.what());
        BOOST_CHECK(ex.file_line());
        BOOST_CHECK(ex.func_name());
        BOOST_CHECK_EQUAL(ex.expression(), "i != 42");
        BOOST_CHECK(ex.message());
    }
    catch (...)
    {
        BOOST_REQUIRE(false);
    }
    BOOST_REQUIRE(ex_thrown);

    ex_thrown = false;
    try
    {
        fn_no_msg();
    }
    catch (const x3me::assert::assert_fail& ex)
    {
        ex_thrown = true;
        BOOST_CHECK(ex.what());
        BOOST_CHECK(ex.file_line());
        BOOST_CHECK(ex.func_name());
        BOOST_CHECK_EQUAL(ex.expression(), "i != 42");
        BOOST_CHECK(!ex.message());
    }
    catch (...)
    {
        BOOST_REQUIRE(false);
    }
    BOOST_REQUIRE(ex_thrown);
}

BOOST_AUTO_TEST_CASE(test_x3me_assert_no_warnings)
{
    // Just few asserts to check the proper compilation without warnings
    // for unused variables
    int k  = 43;
    bool f = true;
    X3ME_ASSERT((k == 43) && (f == true));
    X3ME_ASSERT((k == 43) && (f == true), "With msg");
}

BOOST_AUTO_TEST_CASE(test_x3me_assert)
{
    // X3ME_ASSERT must be active if NDEBUG is not set, but not otherwise.
    // In addition the expression must not be evaluated if NDEBUG is present
    // The presence/absence of the 'message' in the throw exception
    // verifies that the correct enforce has been triggered.
    bool ex_thrown      = false;
    bool expr_evaluated = false;
    auto expr_fun       = [&]
    {
        expr_evaluated = true;
        return false;
    };
    auto expr_fun2 = [&](int, double)
    {
        expr_evaluated = true;
        return false;
    };
    try
    {
        X3ME_ASSERT(expr_fun());
        X3ME_ASSERT(expr_fun2(42, 42.0));
        X3ME_ASSERT(expr_fun(), "With msg");
        X3ME_ASSERT(expr_fun2(42, 42.0), "With msg");
    }
    catch (...)
    {
        BOOST_REQUIRE(false);
    }
    BOOST_REQUIRE(!ex_thrown);
    BOOST_REQUIRE(!expr_evaluated);
}

BOOST_AUTO_TEST_SUITE_END()

#undef X3ME_ASSERT_DO_THROW
