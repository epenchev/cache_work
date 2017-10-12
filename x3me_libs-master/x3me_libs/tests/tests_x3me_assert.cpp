#include <boost/test/unit_test.hpp>

#define X3ME_ASSERT_DO_THROW // We can't unit tests asserts
#include "../x3me_assert.h"

BOOST_AUTO_TEST_SUITE(tests_x3me_assert)

BOOST_AUTO_TEST_CASE(test_x3me_enforce)
{
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

BOOST_AUTO_TEST_CASE(test_x3me_assert)
{
    // X3ME_ASSERT must be active if NDEBUG is not set, but not otherwise.
    // In addition the expression must not be evaluated if NDEBUG is present
    // The presence/absence of the 'message' in the throw exception
    // verifies that the correct enforce has been triggered.
    auto fn_msg = []
    {
        volatile int i = 42;
        X3ME_ASSERT(i == 42); // This must not throw
        X3ME_ASSERT(i != 42, "The 'i' must be 42");
    };
    auto fn_no_msg = []
    {
        volatile int i = 42;
        X3ME_ASSERT(i == 42, "This must not throw");
        X3ME_ASSERT(i != 42);
    };
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

BOOST_AUTO_TEST_SUITE_END()

#undef X3ME_ASSERT_DO_THROW
