#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../xutils/tagged_buffer_queue.h"

BOOST_AUTO_TEST_SUITE(tagged_buffer_queue_tests)

BOOST_AUTO_TEST_CASE(tagged_buffer_create_destroy)
{
    struct test_tag
    {
        bool& destructor_called_;

        test_tag(void*, size_t, bool& f) : destructor_called_(f)
        {
            destructor_called_ = false;
        }
        ~test_tag() noexcept { destructor_called_ = true; }
    };

    constexpr size_t bufsize = 64;
    bool destructor_called   = false;
    {
        auto p =
            xutils::tagged_buffer<test_tag>::create(bufsize, destructor_called);
        BOOST_ASSERT(p);
    }
    BOOST_CHECK(destructor_called);
}

BOOST_AUTO_TEST_CASE(tagged_buffer_buffer)
{
    constexpr size_t bufsize = 16;

    struct test_tag
    {
        char* str_ = nullptr;

        test_tag(void* buff, size_t size, const char* str)
        {
            BOOST_CHECK_EQUAL(size, bufsize);

            str_ = static_cast<char*>(buff);
            strncpy(str_, str, size);
        }
    };

    constexpr char str[] = "Hello buffer!!!";
    static_assert(sizeof(str) == bufsize, "Need to fill the whole buffer");
    auto p = xutils::tagged_buffer<test_tag>::create(bufsize, str);
    BOOST_ASSERT(p);
    BOOST_CHECK(strcmp(p->str_, str) == 0);
}

BOOST_AUTO_TEST_CASE(tagged_buffer_queue_push_pop)
{
    constexpr size_t bufsize = 8;

    struct test_tag
    {
        char* str_ = nullptr;

        test_tag(void* buff, size_t size, const char* str)
        {
            BOOST_CHECK_EQUAL(size, bufsize);

            str_ = static_cast<char*>(buff);
            strncpy(str_, str, size);
        }
    };

    int nums[] = {12345, 56789, 12345, 56789};
    char str[bufsize];

    using namespace xutils;
    tagged_buffer_queue<test_tag> queue;

    for (int i : nums)
    {
        snprintf(str, bufsize, "%d", i);
        queue.push(tagged_buffer<test_tag>::create(bufsize, str));
    }
    for (int i : nums)
    {
        auto p = queue.pop();
        BOOST_ASSERT(p);
        BOOST_CHECK_EQUAL(i, atoi(p->str_));
    }
}

BOOST_AUTO_TEST_CASE(tagged_buffer_queue_emplace_pop)
{
    constexpr size_t bufsize = 10;

    struct test_tag
    {
        char* str_ = nullptr;

        test_tag(void* buff, size_t size, const char* str)
        {
            BOOST_CHECK_EQUAL(size, bufsize);

            str_ = static_cast<char*>(buff);
            strncpy(str_, str, size);
        }
    };

    int nums[] = {123456789, 567891234, 123456789, 567891234};
    char str[bufsize];

    using namespace xutils;
    tagged_buffer_queue<test_tag> queue;

    for (int i : nums)
    {
        snprintf(str, bufsize, "%d", i);
        queue.emplace(bufsize, str);
    }
    for (int i : nums)
    {
        auto p = queue.pop();
        BOOST_ASSERT(p);
        BOOST_CHECK_EQUAL(i, atoi(p->str_));
    }
}

BOOST_AUTO_TEST_CASE(tagged_buffer_queue_move)
{
    constexpr size_t bufsize = 8;

    struct test_tag
    {
        char* str_ = nullptr;
        int i_     = 0;

        test_tag(void* buff, size_t size, const char* str, int i) : i_(i)
        {
            BOOST_CHECK_EQUAL(size, bufsize);

            str_ = static_cast<char*>(buff);
            strncpy(str_, str, size);
        }
    };

    int nums[] = {12345, 56789, 12345, 56789};
    char str[bufsize];

    using namespace xutils;
    tagged_buffer_queue<test_tag> queue;

    for (int i : nums)
    {
        snprintf(str, bufsize, "%d", i);
        queue.emplace(bufsize, str, i);
    }
    BOOST_CHECK(!queue.empty());
    BOOST_CHECK_EQUAL(queue.size(), sizeof(nums) / sizeof(nums[0]));

    auto queue2(std::move(queue)); // Move construction
    BOOST_CHECK(queue.empty());
    BOOST_CHECK_EQUAL(queue.size(), 0);
    BOOST_CHECK(!queue2.empty());
    BOOST_CHECK_EQUAL(queue2.size(), sizeof(nums) / sizeof(nums[0]));

    decltype(queue2) queue3;
    queue3 = std::move(queue2); // Move assignment
    BOOST_CHECK(queue2.empty());
    BOOST_CHECK_EQUAL(queue2.size(), 0);
    BOOST_CHECK(!queue3.empty());
    BOOST_CHECK_EQUAL(queue3.size(), sizeof(nums) / sizeof(nums[0]));

    for (int i : nums)
    {
        auto p = queue3.pop();
        BOOST_ASSERT(p);
        BOOST_CHECK_EQUAL(i, atoi(p->str_));
        BOOST_CHECK_EQUAL(i, p->i_);
    }
}

BOOST_AUTO_TEST_CASE(tagged_buffer_queue_swap)
{
    constexpr size_t bufsize = 8;

    struct test_tag
    {
        char* str_ = nullptr;
        int i_     = 0;

        test_tag(void* buff, size_t size, const char* str, int i) : i_(i)
        {
            BOOST_CHECK_EQUAL(size, bufsize);

            str_ = static_cast<char*>(buff);
            strncpy(str_, str, size);
        }
    };

    int nums[] = {12345, 56789, 12345, 56789};
    char str[bufsize];

    using namespace xutils;
    tagged_buffer_queue<test_tag> queue;

    for (int i : nums)
    {
        snprintf(str, bufsize, "%d", i);
        queue.emplace(bufsize, str, i);
    }
    BOOST_CHECK(!queue.empty());
    BOOST_CHECK_EQUAL(queue.size(), sizeof(nums) / sizeof(nums[0]));

    decltype(queue) queue2;
    queue2.swap(queue);
    BOOST_CHECK(queue.empty());
    BOOST_CHECK_EQUAL(queue.size(), 0);
    BOOST_CHECK(!queue2.empty());
    BOOST_CHECK_EQUAL(queue2.size(), sizeof(nums) / sizeof(nums[0]));

    for (int i : nums)
    {
        auto p = queue2.pop();
        BOOST_ASSERT(p);
        BOOST_CHECK_EQUAL(i, atoi(p->str_));
        BOOST_CHECK_EQUAL(i, p->i_);
    }
}

BOOST_AUTO_TEST_SUITE_END()
