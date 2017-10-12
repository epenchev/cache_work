#include <boost/test/unit_test.hpp>

#include "../bencode.h"
#include "../short_alloc.h"

using namespace x3me::bencode;

using arena_t = x3me::mem::stack_arena<4096 /*size*/, 8 /*alignment*/>;
using entry_t = x3me::bencode::entry<arena_t>;

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(tests_bencode)

BOOST_AUTO_TEST_CASE(test_construct)
{
    arena_t arena;
    entry_t entry(arena, entry_string);
    BOOST_CHECK_EQUAL(entry.type(), entry_string);
}

BOOST_AUTO_TEST_SUITE_END()
