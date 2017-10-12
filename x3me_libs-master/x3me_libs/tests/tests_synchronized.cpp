#include <mutex>
#include <type_traits>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "../synchronized.h"
#include "../shared_mutex.h"

using namespace x3me::thread;

using int_arr_t             = std::vector<int>;
using unique_synchronized_t = synchronized<int_arr_t, std::mutex>;
using shared_synchronized_t = synchronized<int_arr_t, shared_mutex>;

////////////////////////////////////////////////////////////////////////////////
// The actual thread safety of the synchronized functionality is not tested
// in these unit tests. I couldn't figure out a way to reliably test it with
// unit tests.

BOOST_AUTO_TEST_SUITE(tests_synchronized)

BOOST_AUTO_TEST_CASE(test_typedefs)
{
    BOOST_CHECK((std::is_same<unique_synchronized_t::locked_t,
                              unique_locked<int_arr_t, std::mutex>>::value));
    BOOST_CHECK((std::is_same<unique_synchronized_t::const_locked_t,
                              unique_locked<int_arr_t, std::mutex>>::value));
    BOOST_CHECK((std::is_same<shared_synchronized_t::locked_t,
                              unique_locked<int_arr_t, shared_mutex>>::value));
    // Use shared_locked as const_locked when it's available
    BOOST_CHECK((std::is_same<shared_synchronized_t::const_locked_t,
                              shared_locked<int_arr_t, shared_mutex>>::value));
}

BOOST_AUTO_TEST_CASE(test_default_construct)
{
    unique_synchronized_t us;
    shared_synchronized_t ss;
}

BOOST_AUTO_TEST_SUITE_END()
