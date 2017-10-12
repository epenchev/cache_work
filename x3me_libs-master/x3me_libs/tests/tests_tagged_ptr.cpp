#include <stdlib.h>

#include <iostream>

#include <boost/test/unit_test.hpp>

#define X3ME_ASSERT_DO_THROW // We want to test the asserts too
#include "../tagged_ptr.h"
#include "../scope_guard.h"

using x3me::mem_utils::tagged_ptr;
using x3me::mem_utils::make_tagged_ptr;

BOOST_AUTO_TEST_SUITE(tests_tagged_ptr)

BOOST_AUTO_TEST_CASE(test_tag_bits)
{
    struct alignas(16) s128_t
    {
    };
    struct alignas(32) s256_t
    {
    };
    struct alignas(64) s512_t
    {
    };
    struct alignas(128) s1024_t
    {
    };

    BOOST_CHECK_EQUAL(tagged_ptr<int8_t>::tag_bits, 0);
    BOOST_CHECK_EQUAL(tagged_ptr<int16_t>::tag_bits, 1);
    BOOST_CHECK_EQUAL(tagged_ptr<int32_t>::tag_bits, 2);
    BOOST_CHECK_EQUAL(tagged_ptr<int64_t>::tag_bits, 3);
    BOOST_CHECK_EQUAL(tagged_ptr<s128_t>::tag_bits, 4);
    BOOST_CHECK_EQUAL(tagged_ptr<s256_t>::tag_bits, 5);
    BOOST_CHECK_EQUAL(tagged_ptr<s512_t>::tag_bits, 6);
    BOOST_CHECK_EQUAL(tagged_ptr<s1024_t>::tag_bits, 7);
}

BOOST_AUTO_TEST_CASE(test_default_construction)
{
    tagged_ptr<int> t;
    BOOST_CHECK(t.get() == nullptr);
    BOOST_CHECK_EQUAL(t.get_tag(), 0);
}

BOOST_AUTO_TEST_CASE(test_nullptr_construction)
{
    tagged_ptr<int> t1{nullptr};
    BOOST_CHECK(t1.get() == nullptr);
    BOOST_CHECK_EQUAL(t1.get_tag(), 0);
    BOOST_CHECK(!t1);
    tagged_ptr<int> t2{nullptr, 0};
    BOOST_CHECK(t2.get() == nullptr);
    BOOST_CHECK_EQUAL(t2.get_tag(), 0);
    BOOST_CHECK(!t2);
    auto t3 = make_tagged_ptr<int>(nullptr);
    BOOST_CHECK(t3.get() == nullptr);
    BOOST_CHECK_EQUAL(t3.get_tag(), 0);
    BOOST_CHECK(!t3);
    auto t4 = make_tagged_ptr<int>(nullptr, 0);
    BOOST_CHECK(t4.get() == nullptr);
    BOOST_CHECK_EQUAL(t4.get_tag(), 0);
    BOOST_CHECK(!t4);
}

BOOST_AUTO_TEST_CASE(test_construction)
{
    const int i[]       = {42, 43};
    const uint8_t tag[] = {0b00000010, 0b00000011};

    auto t0 = make_tagged_ptr(&i[0], tag[0]);
    auto t1 = make_tagged_ptr(&i[1], tag[1]);

    BOOST_CHECK(t0.get() == &i[0]);
    BOOST_CHECK_EQUAL(t0.get_tag(), tag[0]);
    BOOST_CHECK_EQUAL(*t0, i[0]);
    BOOST_CHECK(t0);
    BOOST_CHECK(t1.get() == &i[1]);
    BOOST_CHECK_EQUAL(t1.get_tag(), tag[1]);
    BOOST_CHECK_EQUAL(*t1, i[1]);
    BOOST_CHECK(t1);
}

BOOST_AUTO_TEST_CASE(test_construction_overaligned_data)
{
    // Test setup
    struct alignas(64) test0
    {
        int i = 42;
    };
    struct alignas(128) test1
    {
        int i = 43;
    };
    const uint8_t tag0 = 0b00101010; // We have 6 bits for tag
    const uint8_t tag1 = 0b01101010; // We have 7 bits for tag

    // Let's construct them on even more aligned buffer
    // It must work as expected.
    void* ptr0      = nullptr;
    const auto ret0 = ::posix_memalign(&ptr0, 2048, 128);
    BOOST_REQUIRE(ret0 == 0);
    void* ptr1      = nullptr;
    const auto ret1 = ::posix_memalign(&ptr1, 4096, 128);
    BOOST_REQUIRE(ret1 == 0);

    auto t0 = new (ptr0) test0;
    auto t1 = new (ptr1) test1;
    X3ME_SCOPE_EXIT
    {
        t0->~test0();
        t1->~test1();
        ::free(ptr0);
        ::free(ptr1);
    };

    // Actual testing
    auto pt0 = make_tagged_ptr(t0, tag0);
    auto pt1 = make_tagged_ptr(t1, tag1);

    BOOST_CHECK(pt0.get() == t0);
    BOOST_CHECK_EQUAL(pt0.get_tag(), tag0);
    BOOST_CHECK_EQUAL(pt0->i, t0->i);
    BOOST_CHECK(pt1.get() == t1);
    BOOST_CHECK_EQUAL(pt1.get_tag(), tag1);
    BOOST_CHECK_EQUAL(pt1->i, t1->i);
}

BOOST_AUTO_TEST_CASE(test_reset)
{
    // Test setup
    struct alignas(64) test0
    {
        int i = 42;
    };
    struct alignas(128) test1
    {
        int i = 43;
    };
    const uint8_t tag0 = 0b00101010; // We have 6 bits for tag
    const uint8_t tag1 = 0b01101010; // We have 7 bits for tag

    // Let's construct them on even more aligned buffer
    // It must work as expected.
    void* ptr0      = nullptr;
    const auto ret0 = ::posix_memalign(&ptr0, 2048, 128);
    BOOST_REQUIRE(ret0 == 0);
    void* ptr1      = nullptr;
    const auto ret1 = ::posix_memalign(&ptr1, 4096, 128);
    BOOST_REQUIRE(ret1 == 0);

    auto t0 = new (ptr0) test0;
    auto t1 = new (ptr1) test1;
    X3ME_SCOPE_EXIT
    {
        t0->~test0();
        t1->~test1();
        ::free(ptr0);
        ::free(ptr1);
    };

    // Actual testing
    auto pt0 = make_tagged_ptr(t0, tag0);
    auto pt1 = make_tagged_ptr(t1, tag1);

    BOOST_CHECK(pt0.get() == t0);
    BOOST_CHECK_EQUAL(pt0.get_tag(), tag0);
    BOOST_CHECK_EQUAL(pt0->i, t0->i);
    BOOST_CHECK(pt1.get() == t1);
    BOOST_CHECK_EQUAL(pt1.get_tag(), tag1);
    BOOST_CHECK_EQUAL(pt1->i, t1->i);

    pt0.reset(nullptr, pt0.get_tag()); // preserve the tag
    pt1.reset(nullptr, pt1.get_tag()); // preserve the tag

    BOOST_CHECK(pt0.get() == nullptr);
    BOOST_CHECK_EQUAL(pt0.get_tag(), tag0);
    BOOST_CHECK(pt1.get() == nullptr);
    BOOST_CHECK_EQUAL(pt1.get_tag(), tag1);

    pt0.reset(t0); // Set the pointer, reset the tag
    pt1.reset(t1); // Set the pointer, reset the tag

    BOOST_CHECK(pt0.get() == t0);
    BOOST_CHECK_EQUAL(pt0.get_tag(), 0);
    BOOST_CHECK_EQUAL(pt0->i, t0->i);
    BOOST_CHECK(pt1.get() == t1);
    BOOST_CHECK_EQUAL(pt1.get_tag(), 0);
    BOOST_CHECK_EQUAL(pt1->i, t1->i);

    pt0.reset_tag(tag0); // restore the tag
    pt1.reset_tag(tag1); // restore the tag

    BOOST_CHECK(pt0.get() == t0);
    BOOST_CHECK_EQUAL(pt0.get_tag(), tag0);
    BOOST_CHECK_EQUAL(pt0->i, t0->i);
    BOOST_CHECK(pt1.get() == t1);
    BOOST_CHECK_EQUAL(pt1.get_tag(), tag1);
    BOOST_CHECK_EQUAL(pt1->i, t1->i);

    pt0.reset();
    pt1.reset();
    BOOST_CHECK(pt0.get() == nullptr);
    BOOST_CHECK_EQUAL(pt0.get_tag(), 0);
    BOOST_CHECK(pt1.get() == nullptr);
    BOOST_CHECK_EQUAL(pt1.get_tag(), 0);
}

BOOST_AUTO_TEST_CASE(test_copy_move_assign) // same as the one of regular ptr
{
    struct test
    {
        double d = 42.0;
    };
    const auto tag = 0b00000111;
    auto t         = new test;
    X3ME_SCOPE_EXIT { delete t; };

    auto pt0 = make_tagged_ptr(t, tag);
    BOOST_CHECK_EQUAL(pt0.get(), t);
    BOOST_CHECK_EQUAL(pt0.get_tag(), tag);
    BOOST_CHECK_EQUAL(pt0->d, t->d);
    auto pt1{pt0};
    BOOST_CHECK_EQUAL(pt1.get(), t);
    BOOST_CHECK_EQUAL(pt1.get_tag(), tag);
    BOOST_CHECK_EQUAL(pt1->d, t->d);
    tagged_ptr<test> pt2;
    pt2 = pt0;
    BOOST_CHECK_EQUAL(pt2.get(), t);
    BOOST_CHECK_EQUAL(pt2.get_tag(), tag);
    BOOST_CHECK_EQUAL(pt2->d, t->d);
    auto pt3{std::move(pt0)};
    BOOST_CHECK_EQUAL(pt3.get(), t);
    BOOST_CHECK_EQUAL(pt3.get_tag(), tag);
    BOOST_CHECK_EQUAL(pt3->d, t->d);
    tagged_ptr<test> pt4;
    pt4 = std::move(pt0);
    BOOST_CHECK_EQUAL(pt4.get(), t);
    BOOST_CHECK_EQUAL(pt4.get_tag(), tag);
    BOOST_CHECK_EQUAL(pt4->d, t->d);
    // And the original is still there
    BOOST_CHECK_EQUAL(pt0.get(), t);
    BOOST_CHECK_EQUAL(pt0.get_tag(), tag);
    BOOST_CHECK_EQUAL(pt0->d, t->d);
}

BOOST_AUTO_TEST_CASE(test_with_wrong_values)
{
    struct alignas(64) test
    {
        double d = 42.0;
    };

    int16_t i16 = 9;
    char ch = 0;
    char buff[2 * sizeof(test)];
    char* p = buff;
    if (reinterpret_cast<uintptr_t>(p) % alignof(test) == 0)
        ++p;
    auto wrong_test = (test*)p; // Misaligned
    auto wrong_tag  = 0b01111111; // The allowed tag is 6 bits here we have 7

    tagged_ptr<test> t;

    BOOST_CHECK_THROW(t.reset(wrong_test), x3me::assert::assert_fail);
    BOOST_CHECK_THROW(t.reset(nullptr, wrong_tag), x3me::assert::assert_fail);
    BOOST_CHECK_THROW(t.reset_tag(wrong_tag), x3me::assert::assert_fail);
    BOOST_CHECK(i16 == 9 && ch == 0); // Just to not get optimized out
}

BOOST_AUTO_TEST_CASE(test_swap)
{
    const int i[]       = {42, 43};
    const uint8_t tag[] = {0b00000010, 0b00000011};

    auto t0 = make_tagged_ptr(&i[0], tag[0]);
    auto t1 = make_tagged_ptr(&i[1], tag[1]);

    BOOST_CHECK(t0.get() == &i[0]);
    BOOST_CHECK_EQUAL(t0.get_tag(), tag[0]);
    BOOST_CHECK_EQUAL(*t0, i[0]);
    BOOST_CHECK(t0);
    BOOST_CHECK(t1.get() == &i[1]);
    BOOST_CHECK_EQUAL(t1.get_tag(), tag[1]);
    BOOST_CHECK_EQUAL(*t1, i[1]);
    BOOST_CHECK(t1);

    t0.swap(t1);

    BOOST_CHECK(t1.get() == &i[0]);
    BOOST_CHECK_EQUAL(t1.get_tag(), tag[0]);
    BOOST_CHECK_EQUAL(*t1, i[0]);
    BOOST_CHECK(t0.get() == &i[1]);
    BOOST_CHECK_EQUAL(t0.get_tag(), tag[1]);
    BOOST_CHECK_EQUAL(*t0, i[1]);

    // and swap back
    x3me::mem_utils::swap(t0, t1);

    BOOST_CHECK(t0.get() == &i[0]);
    BOOST_CHECK_EQUAL(t0.get_tag(), tag[0]);
    BOOST_CHECK_EQUAL(*t0, i[0]);
    BOOST_CHECK(t1.get() == &i[1]);
    BOOST_CHECK_EQUAL(t1.get_tag(), tag[1]);
    BOOST_CHECK_EQUAL(*t1, i[1]);
}

BOOST_AUTO_TEST_CASE(test_set_tag_bits)
{
    uint64_t i = 42;
    auto t     = make_tagged_ptr(&i, 0b00000111); // We have 3 bits for tagging

    BOOST_CHECK_EQUAL(t.get_tag(), 0b00000111);
    BOOST_CHECK_EQUAL(t.tag_bit<0>(), true);
    BOOST_CHECK_EQUAL(t.tag_bit<1>(), true);
    BOOST_CHECK_EQUAL(t.tag_bit<2>(), true);

    t.set_tag_bit<0>(false);
    BOOST_CHECK_EQUAL(t.get_tag(), 0b00000110);
    BOOST_CHECK_EQUAL(t.tag_bit<0>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<1>(), true);
    BOOST_CHECK_EQUAL(t.tag_bit<2>(), true);

    t.set_tag_bit<2>(false);
    BOOST_CHECK_EQUAL(t.get_tag(), 0b00000010);
    BOOST_CHECK_EQUAL(t.tag_bit<0>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<1>(), true);
    BOOST_CHECK_EQUAL(t.tag_bit<2>(), false);

    t.set_tag_bit<1>(false);
    BOOST_CHECK_EQUAL(t.get_tag(), 0b00000000);
    BOOST_CHECK_EQUAL(t.tag_bit<0>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<1>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<2>(), false);

    t.set_tag_bit<2>(true);
    BOOST_CHECK_EQUAL(t.get_tag(), 0b00000100);
    BOOST_CHECK_EQUAL(t.tag_bit<0>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<1>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<2>(), true);

    t.set_tag_bit<0>(true);
    BOOST_CHECK_EQUAL(t.get_tag(), 0b00000101);
    BOOST_CHECK_EQUAL(t.tag_bit<0>(), true);
    BOOST_CHECK_EQUAL(t.tag_bit<1>(), false);
    BOOST_CHECK_EQUAL(t.tag_bit<2>(), true);
}

BOOST_AUTO_TEST_CASE(test_comparisson)
{
    const int i[]       = {42, 43};
    const uint8_t tag[] = {0b00000010, 0b00000011};

    auto t00 = make_tagged_ptr(&i[0], tag[0]);
    auto t01 = make_tagged_ptr(&i[0], tag[0]);
    auto t10 = make_tagged_ptr(&i[1], tag[1]);
    auto t11 = make_tagged_ptr(&i[1], tag[1]);

    BOOST_CHECK(t00 == t01);
    BOOST_CHECK(t00 != t10);
    BOOST_CHECK(t00 <= t01);
    BOOST_CHECK(t00 <= t10);
    BOOST_CHECK(t00 < t10);
    BOOST_CHECK(t10 > t00);
    BOOST_CHECK(t10 >= t00);
    BOOST_CHECK(t10 >= t11);
}

BOOST_AUTO_TEST_SUITE_END()

#undef X3ME_ASSERT_DO_THROW
