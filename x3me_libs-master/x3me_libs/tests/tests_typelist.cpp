#include <array>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "../mpl.h"

using namespace x3me::mpl;

BOOST_AUTO_TEST_SUITE(tests_typelist) 

BOOST_AUTO_TEST_CASE(max_size_type)
{
	typedef std::array<char, 40> array_t;
	typedef std::vector<char> vector_t;

	BOOST_CHECK_EQUAL(sizeof(typelist<uint8_t, uint64_t, int32_t, int16_t>::max_size_type_t), 8); 
	BOOST_CHECK_EQUAL(sizeof(typelist<array_t, vector_t, std::string>::max_size_type_t), sizeof(array_t));
	BOOST_CHECK_EQUAL(sizeof(typelist<std::string>::max_size_type_t), sizeof(std::string));
	BOOST_CHECK_EQUAL(sizeof(typelist<>::max_size_type_t), 1);
}

BOOST_AUTO_TEST_CASE(length)
{
	typedef std::array<char, 40> array_t;
	typedef std::vector<char> vector_t;

	BOOST_CHECK_EQUAL((typelist<uint8_t, uint64_t, int32_t, int16_t, double, int, long>::length), 7); 
	BOOST_CHECK_EQUAL((typelist<array_t, vector_t, int32_t, std::string>::length), 4);
	BOOST_CHECK_EQUAL((typelist<char>::length), 1);
	BOOST_CHECK_EQUAL((typelist<>::length), 0);
}

BOOST_AUTO_TEST_SUITE_END( )
