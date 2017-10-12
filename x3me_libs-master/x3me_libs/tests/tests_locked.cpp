#include <mutex>
#include <type_traits>

#include <boost/test/unit_test.hpp>

#include "../locked.h"
#include "../shared_mutex.h"

using namespace x3me::thread;

////////////////////////////////////////////////////////////////////////////////
// The actual locking functionality is not really tested, because I couldn't
// figure out reliable multi-threaded testing of the functionality.
// I mean, the fact that a MT test passes now, doesn't mean that
// it'll always pass.

BOOST_AUTO_TEST_SUITE(tests_locked)

BOOST_AUTO_TEST_CASE(test_unique_locked_typedefs)
{
    {
        uint64_t i = 42;
        std::mutex mut;
        {
            unique_locked<uint64_t, std::mutex> lkd(i, mut);
            BOOST_CHECK((std::is_same<decltype(lkd)::data_t, uint64_t>::value));
        }
        {
            auto lkd = make_unique_locked(i, mut);
            BOOST_CHECK((std::is_same<decltype(lkd)::data_t, uint64_t>::value));
        }
    }
    {
        const uint64_t i = 42;
        std::mutex mut;
        {
            unique_locked<const uint64_t, std::mutex> lkd(i, mut);
            BOOST_CHECK(
                (std::is_same<decltype(lkd)::data_t, const uint64_t>::value));
        }
        {
            auto lkd = make_unique_locked(i, mut);
            BOOST_CHECK(
                (std::is_same<decltype(lkd)::data_t, const uint64_t>::value));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_shared_locked_typedefs)
{
    // The shared_locked data is always const
    {
        uint64_t i = 42;
        shared_mutex mut;
        {
            shared_locked<uint64_t, shared_mutex> lkd(i, mut);
            BOOST_CHECK(
                (std::is_same<decltype(lkd)::data_t, const uint64_t>::value));
        }
        {
            auto lkd = make_shared_locked(i, mut);
            BOOST_CHECK(
                (std::is_same<decltype(lkd)::data_t, const uint64_t>::value));
        }
    }
    {
        const uint64_t i = 42;
        shared_mutex mut;
        {
            shared_locked<const uint64_t, shared_mutex> lkd(i, mut);
            BOOST_CHECK(
                (std::is_same<decltype(lkd)::data_t, const uint64_t>::value));
        }
        {
            auto lkd = make_shared_locked(i, mut);
            BOOST_CHECK(
                (std::is_same<decltype(lkd)::data_t, const uint64_t>::value));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_unique_locked_move_construction)
{
    uint64_t i = 42;
    shared_mutex mut; // Shared mutex can be used too, for unique_locked

    auto lkd1 = make_unique_locked(i, mut);
    BOOST_CHECK(lkd1);

    decltype(lkd1) lkd2(std::move(lkd1));

    BOOST_CHECK(!lkd1);
    BOOST_CHECK(lkd2);
}

BOOST_AUTO_TEST_CASE(test_unique_locked_move_assignment)
{
    uint64_t i = 42;
    shared_mutex mut; // Shared mutex can be used too, for unique_locked

    auto lkd1 = make_unique_locked(i, mut);
    BOOST_CHECK(lkd1);

    decltype(lkd1) lkd2;
    BOOST_CHECK(!lkd2);

    lkd2 = std::move(lkd1);
    BOOST_CHECK(!lkd1);
    BOOST_CHECK(lkd2);
}

BOOST_AUTO_TEST_CASE(test_shared_locked_move_construction)
{
    uint64_t i = 42;
    shared_mutex mut;

    auto lkd1 = make_shared_locked(i, mut);
    BOOST_CHECK(lkd1);

    decltype(lkd1) lkd2(std::move(lkd1));

    BOOST_CHECK(!lkd1);
    BOOST_CHECK(lkd2);
}

BOOST_AUTO_TEST_CASE(test_shared_locked_move_assignment)
{
    uint64_t i = 42;
    shared_mutex mut;

    auto lkd1 = make_shared_locked(i, mut);
    BOOST_CHECK(lkd1);

    decltype(lkd1) lkd2;
    BOOST_CHECK(!lkd2);

    lkd2 = std::move(lkd1);
    BOOST_CHECK(!lkd1);
    BOOST_CHECK(lkd2);
}

BOOST_AUTO_TEST_SUITE_END()
