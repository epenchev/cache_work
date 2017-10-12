#include <memory>
#include <string>

#include <boost/test/unit_test.hpp>

#include "../inplace_fn.h"

using x3me::utils::inplace_params;
using x3me::utils::inplace_fn;

using ref_cnt_t = std::shared_ptr<int>;
auto make_ref_cnt()
{
    return std::make_shared<int>(0);
}

template <typename Fn>
struct fn : Fn
{
    ref_cnt_t ref_cnt_;

    fn() = delete;
    ~fn() = default;

    fn(const Fn& f, const ref_cnt_t& rc) : Fn(f), ref_cnt_(rc) {}
    fn(Fn&& f, const ref_cnt_t& rc) : Fn(std::move(f)), ref_cnt_(rc) {}

    fn(const fn&) = default;
    fn(fn&& rhs) = default;

    fn& operator=(const fn&) = delete;
    fn& operator=(fn&&) = delete;
};

template <typename Fn>
auto make_fn(const ref_cnt_t& rc, Fn&& f)
{
    return fn<std::remove_reference_t<Fn>>(std::forward<Fn>(f), rc);
}

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(tests_inplace_fn)

BOOST_AUTO_TEST_CASE(test_default_construct)
{
    inplace_fn<inplace_params<8>, void()> fn;
    BOOST_CHECK(!fn);
}

BOOST_AUTO_TEST_CASE(test_nullptr_construct)
{
    inplace_fn<inplace_params<8>, void()> fn(nullptr);
    BOOST_CHECK(!fn);
}

BOOST_AUTO_TEST_CASE(test_fn_construct)
{
    int i = 0;
    inplace_fn<inplace_params<8>, void()> fn1([&]
                                              {
                                                  ++i;
                                              });
    BOOST_REQUIRE(fn1);
    BOOST_CHECK_EQUAL(i, 0);
    fn1();
    BOOST_CHECK_EQUAL(i, 1);
}

BOOST_AUTO_TEST_CASE(test_copy_construct)
{
    int i   = 0;
    auto rc = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rc, [&]
                                                       {
                                                           ++i;
                                                       }));
    auto fn2(fn1);
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 2); // This one and the two copies
    BOOST_REQUIRE(fn1);
    BOOST_REQUIRE(fn2);
    fn1();
    fn2();
    BOOST_CHECK_EQUAL(i, 2);
}

BOOST_AUTO_TEST_CASE(test_move_construct)
{
    int i   = 0;
    auto rc = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rc, [&]
                                                       {
                                                           ++i;
                                                       }));
    auto fn2(std::move(fn1));
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 1); // This one and one fn
    BOOST_CHECK(!fn1);
    BOOST_REQUIRE(fn2);
    fn2();
    BOOST_CHECK_EQUAL(i, 1);
}

BOOST_AUTO_TEST_CASE(test_assign_nullptr)
{
    int i   = 0;
    auto rc = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rc, [&]
                                                       {
                                                           ++i;
                                                       }));
    auto fn2(fn1);
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 2); // This one and the two copies
    BOOST_CHECK(fn1);
    BOOST_REQUIRE(fn2);
    fn1 = nullptr;
    BOOST_CHECK(!fn1);
    fn2();
    BOOST_CHECK_EQUAL(i, 1);
}

BOOST_AUTO_TEST_CASE(test_copy_assign)
{
    int i   = 0;
    auto rc = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rc, [&]
                                                       {
                                                           ++i;
                                                       }));
    decltype(fn1) fn2;
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 1); // This one and the one fn
    fn2 = fn1;
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 2); // This one and the two fn
    BOOST_REQUIRE(fn1);
    BOOST_REQUIRE(fn2);
    fn1();
    fn2();
    BOOST_CHECK_EQUAL(i, 2);
}

BOOST_AUTO_TEST_CASE(test_move_assign)
{
    int i   = 0;
    auto rc = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rc, [&]
                                                       {
                                                           ++i;
                                                       }));
    decltype(fn1) fn2;
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 1); // This one and the one fn
    fn2 = std::move(fn1);
    BOOST_CHECK_EQUAL(rc.use_count(), 1 + 1); // This one and the new one
    BOOST_CHECK(!fn1);
    BOOST_REQUIRE(fn2);
    fn2();
    BOOST_CHECK_EQUAL(i, 1);
}

BOOST_AUTO_TEST_CASE(test_assign_fn)
{
    int i = 0;
    std::string s;
    auto rci = make_ref_cnt();
    auto rcs = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rci, [&]
                                                       {
                                                           ++i;
                                                       }));
    decltype(fn1) fn2;
    BOOST_CHECK_EQUAL(rci.use_count(), 1 + 1); // This one and the one fn
    fn2 = fn1;
    BOOST_CHECK_EQUAL(rci.use_count(), 1 + 2); // This one and the two fn
    fn2 = make_fn(rcs, [&]
                  {
                      s = "42";
                  });
    BOOST_CHECK_EQUAL(rci.use_count(), 1 + 1); // This one and the one fn
    BOOST_CHECK_EQUAL(rcs.use_count(), 1 + 1); // This one and the one fn
    BOOST_REQUIRE(fn1);
    BOOST_REQUIRE(fn2);
    fn1();
    fn2();
    BOOST_CHECK_EQUAL(i, 1);
    BOOST_CHECK_EQUAL(s, "42");
}

BOOST_AUTO_TEST_CASE(test_swap)
{
    int i = 0;
    std::string s;
    auto rci = make_ref_cnt();
    auto rcs = make_ref_cnt();
    inplace_fn<inplace_params<24>, void()> fn1(make_fn(rci, [&]
                                                       {
                                                           ++i;
                                                       }));
    inplace_fn<inplace_params<24>, void()> fn2(make_fn(rcs, [&]
                                                       {
                                                           s += "42";
                                                       }));
    BOOST_CHECK_EQUAL(rci.use_count(), 1 + 1); // This one and the one fn
    BOOST_CHECK_EQUAL(rcs.use_count(), 1 + 1); // This one and the one fn
    fn1();
    BOOST_CHECK_EQUAL(i, 1);
    BOOST_CHECK_EQUAL(s, ""); // 's' must not be changed
    fn2();
    BOOST_CHECK_EQUAL(i, 1); // 'i' must not be changed
    BOOST_CHECK_EQUAL(s, "42");
    fn1.swap(fn2);
    // The use counts must be the same
    BOOST_CHECK_EQUAL(rci.use_count(), 1 + 1); // This one and the one fn
    BOOST_CHECK_EQUAL(rcs.use_count(), 1 + 1); // This one and the one fn
    fn1();
    BOOST_CHECK_EQUAL(i, 1); // 'i' must not be changed
    BOOST_CHECK_EQUAL(s, "4242");
    fn2();
    BOOST_CHECK_EQUAL(i, 2);
    BOOST_CHECK_EQUAL(s, "4242"); // 's' must not be changed
}

BOOST_AUTO_TEST_CASE(test_call)
{
    struct alignas(16) fun
    {
        mutable int* called;
        mutable int* const_called;

        void operator()(const char*, int) { *called += 1; }
        void operator()(const char*, int) const { *const_called += 1; }
    };

    int called       = 0;
    int const_called = 0;

    inplace_fn<inplace_params<16, 16>, void(const char*, int)> fn(
        fun{&called, &const_called});
    using fn_t = decltype(fn);

    BOOST_CHECK_EQUAL(called, 0);
    BOOST_CHECK_EQUAL(const_called, 0);
    fn("Hello", 42); // non const call
    BOOST_CHECK_EQUAL(called, 1);
    BOOST_CHECK_EQUAL(const_called, 0);
    const_cast<const fn_t&>(fn)("Hello", 42);
    BOOST_CHECK_EQUAL(called, 1);
    BOOST_CHECK_EQUAL(const_called, 1);
}

BOOST_AUTO_TEST_SUITE_END()
