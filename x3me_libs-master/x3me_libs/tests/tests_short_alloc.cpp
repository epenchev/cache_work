#include <boost/test/unit_test.hpp>

#include <iostream>
#include "../short_alloc.h"

using namespace x3me::mem;

namespace
{

template <typename Arena>
void arena_alloc_in_buff(Arena& arena)
{
    enum
    {
        alignment = Arena::alignment
    };
    static_assert(alignment == 8, "");
    assert(arena.size() == 128);
    auto p1 = arena.allocate(1);
    BOOST_CHECK_EQUAL((size_t)p1 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 1 * alignment);
    auto p2 = arena.allocate(2);
    BOOST_CHECK_GE(p2, p1);
    BOOST_CHECK_EQUAL((size_t)p2 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 2 * alignment);
    auto p3 = arena.allocate(4);
    BOOST_CHECK_GE(p3, p2);
    BOOST_CHECK_EQUAL((size_t)p3 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 3 * alignment);
    auto p4 = arena.allocate(8);
    BOOST_CHECK_GE(p4, p3);
    BOOST_CHECK_EQUAL((size_t)p4 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 4 * alignment);
    auto p5 = arena.allocate(12);
    BOOST_CHECK_GE(p5, p4);
    BOOST_CHECK_EQUAL((size_t)p5 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 6 * alignment);
    auto p6 = arena.allocate(4);
    BOOST_CHECK_GE(p6, p5);
    BOOST_CHECK_EQUAL((size_t)p6 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 7 * alignment);
    auto p7 = arena.allocate(8);
    BOOST_CHECK_GE(p7, p6);
    BOOST_CHECK_EQUAL((size_t)p7 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 8 * alignment);
    auto p8 = arena.allocate(1);
    BOOST_CHECK_GE(p8, p7);
    BOOST_CHECK_EQUAL((size_t)p8 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 9 * alignment);
}

template <typename Arena>
void arena_alloc_out_buff(Arena& arena)
{
    enum
    {
        alignment = Arena::alignment
    };
    static_assert(alignment == 8, "");
    assert(arena.size() == 16);
    auto p1 = arena.allocate(8);
    BOOST_CHECK_EQUAL((size_t)p1 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 1 * alignment);
    auto p2 = arena.allocate(1);
    BOOST_CHECK_GE(p2, p1);
    BOOST_CHECK_EQUAL((size_t)p2 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 2 * alignment);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    auto p3 = arena.allocate(7);
    BOOST_CHECK_EQUAL((size_t)p3 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    auto p4 = arena.allocate(15);
    BOOST_CHECK_EQUAL((size_t)p4 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    // These are not yet unit tested, but still, don't leak.
    arena.deallocate(p3, 7);
    arena.deallocate(p4, 15);
}

template <typename Arena>
void arena_dealloc_in_buff(Arena& arena)
{
    enum
    {
        alignment = Arena::alignment
    };
    static_assert(alignment == 8, "");
    assert(arena.size() == 128);
    auto p1 = arena.allocate(1);
    BOOST_CHECK_EQUAL((size_t)p1 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 1 * alignment);
    auto p2 = arena.allocate(2);
    BOOST_CHECK_GE(p2, p1);
    BOOST_CHECK_EQUAL((size_t)p2 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 2 * alignment);
    // Deallocate from the end
    arena.deallocate(p2, 2);
    BOOST_CHECK_EQUAL(arena.used(), 1 * alignment);
    auto p3 = arena.allocate(15);
    BOOST_CHECK_GE(p3, p1);
    BOOST_CHECK_EQUAL((size_t)p3 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 3 * alignment);
    auto p4 = arena.allocate(7);
    BOOST_CHECK_GE(p4, p3);
    BOOST_CHECK_EQUAL((size_t)p4 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 4 * alignment);
    // Deallocate not from the end doesn't change the used memory
    arena.deallocate(p3, 15);
    BOOST_CHECK_EQUAL(arena.used(), 4 * alignment);
    // Deallocate from the end
    arena.deallocate(p4, 7);
    BOOST_CHECK_EQUAL(arena.used(), 3 * alignment);
}

template <typename Arena>
void arena_dealloc_out_buff(Arena& arena)
{
    enum
    {
        alignment = Arena::alignment
    };
    static_assert(alignment == 8, "");
    assert(arena.size() == 16);
    auto p1 = arena.allocate(9);
    BOOST_CHECK_EQUAL((size_t)p1 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 2 * alignment);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    // The arena is full, allocate memory from outside it
    auto p2 = arena.allocate(2);
    BOOST_CHECK_EQUAL((size_t)p2 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    auto p3 = arena.allocate(23);
    BOOST_CHECK_EQUAL((size_t)p3 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    // Deallocating a memory from outside the arena
    // doesn't change the used arena space
    arena.deallocate(p2, 2);
    BOOST_CHECK_EQUAL(arena.used(), arena.size());
    // Deallocate from the end of the arena space
    arena.deallocate(p1, 9);
    BOOST_CHECK_EQUAL(arena.used(), 0);
    // Allocate again from the arena
    p1 = arena.allocate(7);
    BOOST_CHECK_EQUAL((size_t)p1 % alignment, 0);
    BOOST_CHECK_EQUAL(arena.used(), 1 * alignment);
    // Deallocate the first chunk from outside the arena
    arena.deallocate(p3, 23);
    BOOST_CHECK_EQUAL(arena.used(), 1 * alignment);
}
}

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(tests_short_alloc)

BOOST_AUTO_TEST_CASE(stack_arena_alloc_in_buff)
{
    enum
    {
        size      = 128,
        alignment = 8
    };
    stack_arena<size, alignment> sa;
    arena_alloc_in_buff(sa);
}

BOOST_AUTO_TEST_CASE(stack_arena_alloc_out_buff)
{
    enum
    {
        size      = 16,
        alignment = 8
    };
    stack_arena<size, alignment> sa;
    arena_alloc_out_buff(sa);
}

BOOST_AUTO_TEST_CASE(stack_arena_dealloc_in_buff)
{
    enum
    {
        size      = 128,
        alignment = 8
    };
    stack_arena<size, alignment> sa;
    arena_dealloc_in_buff(sa);
}

BOOST_AUTO_TEST_CASE(stack_arena_dealloc_out_buff)
{
    enum
    {
        size      = 16,
        alignment = 8
    };
    stack_arena<size, alignment> sa;
    arena_dealloc_out_buff(sa);
}

BOOST_AUTO_TEST_CASE(heap_arena_alloc_in_buff)
{
    enum
    {
        size      = 128,
        alignment = 8
    };
    heap_arena<alignment> sa(size);
    arena_alloc_in_buff(sa);
}

BOOST_AUTO_TEST_CASE(heap_arena_alloc_out_buff)
{
    enum
    {
        size      = 16,
        alignment = 8
    };
    heap_arena<alignment> sa(size);
    arena_alloc_out_buff(sa);
}

BOOST_AUTO_TEST_CASE(heap_arena_dealloc_in_buff)
{
    enum
    {
        size      = 128,
        alignment = 8
    };
    heap_arena<alignment> sa(size);
    arena_dealloc_in_buff(sa);
}

BOOST_AUTO_TEST_CASE(heap_arena_dealloc_out_buff)
{
    enum
    {
        size      = 16,
        alignment = 8
    };
    heap_arena<alignment> sa(size);
    arena_dealloc_out_buff(sa);
}

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(short_alloc_comparisson)
{
    using stack_arena_t  = stack_arena<128>;
    using heap_arena_t   = heap_arena<8>;
    using short_alloc1_t = short_alloc<int, stack_arena_t>;
    using short_alloc2_t = short_alloc<int, heap_arena_t>;

    stack_arena_t sa1a;
    short_alloc1_t shalloc1a(sa1a);
    short_alloc1_t shalloc1b(shalloc1a);
    heap_arena_t ha2a(128), ha2b(128);
    short_alloc2_t shalloc2a(ha2a), shalloc2b(ha2b);
    short_alloc2_t shalloc2c(shalloc2a);

    BOOST_CHECK(shalloc1a == shalloc1b);
    BOOST_CHECK(shalloc2a == shalloc2c);
    BOOST_CHECK(shalloc2a != shalloc2b);
}

BOOST_AUTO_TEST_SUITE_END()
