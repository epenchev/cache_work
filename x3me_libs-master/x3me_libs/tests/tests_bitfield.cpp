#include <sstream>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "../bitfield.h"

using bitfield_t      = x3me::bt_utils::bitfield;
using bytes_to_bits_t = x3me::bt_utils::bytes_to_bits;
using bitfield_bits_t = x3me::bt_utils::bitfield_bits;

BOOST_AUTO_TEST_SUITE(tests_bitfield)

BOOST_AUTO_TEST_CASE(test_default_construct)
{
    bitfield_t bf;
    BOOST_CHECK(!bf);
    BOOST_CHECK(bf.empty());
    BOOST_CHECK(!bf.data());
    BOOST_CHECK_EQUAL(bf.size(), 0);
    BOOST_CHECK_EQUAL(bf.byte_size(), 0);
}

BOOST_AUTO_TEST_CASE(test_construct_with_size)
{
    {
        const uint8_t expected[2] = {0xFF, 0b11100000};
        bitfield_t bf(11, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 11);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, 2) == 0);
    }
    {
        const uint8_t expected[2] = {0x00, 0x00};
        bitfield_t bf(11, false);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 11);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
}

BOOST_AUTO_TEST_CASE(test_construct_from_bytes)
{
    const uint8_t input[3]    = {0x00, 0x01, 0x01};
    const uint8_t expected[1] = {0b01100000};
    bitfield_t bf(bytes_to_bits_t(input, sizeof(input)));
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 3);
    BOOST_CHECK_EQUAL(bf.byte_size(), 1);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_construct_from_bits)
{
    const uint8_t input[2]    = {0b10011001, 0b11100011};
    const uint8_t expected[2] = {0b10011001, 0b11100000};
    bitfield_t bf(bitfield_bits_t(input, 14));
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 14);
    BOOST_CHECK_EQUAL(bf.byte_size(), 2);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_move_construct)
{
    const uint8_t input[9] = {0x00, 0x01, 0x01, 0x00, 0x00,
                              0x01, 0x01, 0x01, 0x01};
    const uint8_t expected[2] = {0b01100111, 0b10000000};
    bitfield_t tmp(bytes_to_bits_t(input, sizeof(input)));
    bitfield_t bf(std::move(tmp));
    BOOST_CHECK(!tmp);
    BOOST_CHECK(tmp.empty());
    BOOST_CHECK(!tmp.data());
    BOOST_CHECK_EQUAL(tmp.size(), 0);
    BOOST_CHECK_EQUAL(tmp.byte_size(), 0);
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 9);
    BOOST_CHECK_EQUAL(bf.byte_size(), 2);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_move_assignment)
{
    const uint8_t input[9] = {0x00, 0x01, 0x01, 0x00, 0x00,
                              0x01, 0x01, 0x01, 0x01};
    const uint8_t expected[2] = {0b01100111, 0b10000000};
    bitfield_t tmp(bytes_to_bits_t(input, sizeof(input)));
    bitfield_t bf;
    bf = std::move(tmp);
    BOOST_CHECK(!tmp);
    BOOST_CHECK(tmp.empty());
    BOOST_CHECK(!tmp.data());
    BOOST_CHECK_EQUAL(tmp.size(), 0);
    BOOST_CHECK_EQUAL(tmp.byte_size(), 0);
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 9);
    BOOST_CHECK_EQUAL(bf.byte_size(), 2);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_resize_from_zero_size)
{
    { // Resize to size bigger than 1 byte
        const uint8_t expected[2] = {0xFF, 0b11100000};
        bitfield_t bf;
        bf.resize(11, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 11);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
    { // Resize to size lesser than 1 byte
        const uint8_t expected[1] = {0b11111100};
        bitfield_t bf;
        bf.resize(6, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 6);
        BOOST_CHECK_EQUAL(bf.byte_size(), 1);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
    { // Resize to size lesser than 1 byte
        const uint8_t expected[1] = {0x00};
        bitfield_t bf;
        bf.resize(4, false);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 4);
        BOOST_CHECK_EQUAL(bf.byte_size(), 1);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
}

BOOST_AUTO_TEST_CASE(test_resize_to_bigger_size)
{
    { // Resize changing the byte size
        const uint8_t expected[3] = {0xFF, 0b11111111, 0b10000000};
        bitfield_t bf(11, true);
        bf.resize(17, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 17);
        BOOST_CHECK_EQUAL(bf.byte_size(), 3);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
    { // Resize without changing the byte size
        const uint8_t expected[2] = {0xFF, 0b11111100};
        bitfield_t bf(11, true);
        bf.resize(14, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 14);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
    { // Resize changing the byte size
        const uint8_t expected[2] = {0xFF, 0x00};
        bitfield_t bf(8, true);
        bf.resize(16, false);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 16);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
}

BOOST_AUTO_TEST_CASE(test_resize_to_smaller_size)
{
    { // Resize changing the byte size
        const uint8_t expected[2] = {0xFF, 0b11100000};
        bitfield_t bf(22, true);
        bf.resize(11, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 11);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
    { // Resize without changing the byte size
        const uint8_t expected[2] = {0xFF, 0b11111100};
        bitfield_t bf(15, true);
        bf.resize(14, true);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 14);
        BOOST_CHECK_EQUAL(bf.byte_size(), 2);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
    { // Resize changing the byte size
        const uint8_t expected[1] = {0b11111110};
        bitfield_t bf(16, true);
        bf.resize(7, false);
        BOOST_CHECK(bf);
        BOOST_CHECK(!bf.empty());
        BOOST_CHECK_EQUAL(bf.size(), 7);
        BOOST_CHECK_EQUAL(bf.byte_size(), 1);
        BOOST_REQUIRE(bf.data());
        BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
    }
}

BOOST_AUTO_TEST_CASE(test_assign_bytes)
{
    const uint8_t input[3]    = {0x00, 0x01, 0x01};
    const uint8_t expected[1] = {0b01100000};
    bitfield_t bf(78, true);
    BOOST_CHECK_EQUAL(bf.size(), 78);
    bf.assign(bytes_to_bits_t(input, sizeof(input)));
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 3);
    BOOST_CHECK_EQUAL(bf.byte_size(), 1);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_assign_bits)
{
    const uint8_t input[2]    = {0b10011001, 0b11100011};
    const uint8_t expected[2] = {0b10011001, 0b11100000};
    bitfield_t bf(87, true);
    BOOST_CHECK_EQUAL(bf.size(), 87);
    bf.assign(bitfield_bits_t(input, 14));
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 14);
    BOOST_CHECK_EQUAL(bf.byte_size(), 2);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_append_bits)
{
    const uint8_t input[3]    = {0b01010100, 0b01110000, 0b01100000};
    const uint8_t expected[3] = {0b11111111, 0b01010101, 0b11001100};
    bitfield_t bf(8, true);
    // Append at position multiple of 8
    bf.append(bitfield_bits_t(&input[0], 6));
    // Append at position not mulitple of 8
    bf.append(bitfield_bits_t(&input[1], 5));
    // Append at position not mulitple of 8, and in the same byte
    bf.append(bitfield_bits_t(&input[2], 3));
    BOOST_CHECK(bf);
    BOOST_CHECK(!bf.empty());
    BOOST_CHECK_EQUAL(bf.size(), 22);
    BOOST_CHECK_EQUAL(bf.byte_size(), 3);
    BOOST_REQUIRE(bf.data());
    BOOST_CHECK(std::memcmp(bf.data(), expected, sizeof(expected)) == 0);
}

BOOST_AUTO_TEST_CASE(test_clear)
{
    bitfield_t bf(8, true);
    BOOST_CHECK_EQUAL(bf.size(), 8);
    bf.clear();
    BOOST_CHECK(!bf);
    BOOST_CHECK(bf.empty());
    BOOST_CHECK(!bf.data());
    BOOST_CHECK_EQUAL(bf.size(), 0);
    BOOST_CHECK_EQUAL(bf.byte_size(), 0);
}

BOOST_AUTO_TEST_CASE(test_set_get_bit)
{
    bitfield_t bf(15, true);
    bf.set_bit(14, false);
    bf.set_bit(12, false);
    bf.set_bit(8, false);
    bf.set_bit(7, false);
    bf.set_bit(0, false);
    BOOST_CHECK_EQUAL(bf.bit(14), false);
    BOOST_CHECK_EQUAL(bf.bit(12), false);
    BOOST_CHECK_EQUAL(bf.bit(8), false);
    BOOST_CHECK_EQUAL(bf.bit(7), false);
    BOOST_CHECK_EQUAL(bf.bit(0), false);
    bf.set_bit(0, true);
    bf.set_bit(8, true);
    bf.set_bit(14, true);
    BOOST_CHECK_EQUAL(bf.bit(14), true);
    BOOST_CHECK_EQUAL(bf.bit(12), false);
    BOOST_CHECK_EQUAL(bf.bit(8), true);
    BOOST_CHECK_EQUAL(bf.bit(7), false);
    BOOST_CHECK_EQUAL(bf.bit(0), true);
}

BOOST_AUTO_TEST_CASE(test_all_any_none)
{
    uint8_t input[] = {0b00100000, 0b00000000};
    bitfield_t bf(13, true);
    BOOST_CHECK(bf.all() == true);
    BOOST_CHECK(bf.any() == true);
    BOOST_CHECK(bf.none() == false);
    bf.set_bit(6, false);
    BOOST_CHECK(bf.all() == false);
    BOOST_CHECK(bf.any() == true);
    BOOST_CHECK(bf.none() == false);
    bf.assign(bitfield_bits_t(input, 13));
    BOOST_CHECK(bf.all() == false);
    BOOST_CHECK(bf.any() == true);
    BOOST_CHECK(bf.none() == false);
    input[0] = 0x00;
    bf.assign(bitfield_bits_t(input, 13));
    BOOST_CHECK(bf.all() == false);
    BOOST_CHECK(bf.any() == false);
    BOOST_CHECK(bf.none() == true);

    std::vector<uint8_t> v(666, 1);
    bitfield_t bf1;
    bf1.assign(bytes_to_bits_t(v));
    BOOST_CHECK(bf1.all() == true);
    BOOST_CHECK(bf1.any() == true);
    BOOST_CHECK(bf1.none() == false);
}

BOOST_AUTO_TEST_CASE(test_streaming)
{
    const uint8_t input[] = {0xFC, 0x42};
    bitfield_t bf;
    {
        std::stringstream ss;
        ss << bf;
        BOOST_CHECK_EQUAL(ss.str(), "0[]");
    }
    bf.assign(bitfield_bits_t(input, 16));
    {
        std::stringstream ss;
        ss << bf;
        BOOST_CHECK_EQUAL(ss.str(), "16[FC42]");
    }
}

BOOST_AUTO_TEST_SUITE_END()
