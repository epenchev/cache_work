#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>

#include <boost/algorithm/hex.hpp>
#include <boost/test/unit_test.hpp>

#include "common.h"

#include "../infohash.h"

using namespace x3me::bt_utils;

BOOST_AUTO_TEST_SUITE(tests_infohash_view) 

BOOST_AUTO_TEST_CASE(test_default_construction)
{
	infohash_view ihv;
	BOOST_CHECK(ihv.data() == nullptr);
	BOOST_CHECK_EQUAL(ihv.empty(), true); 
}

BOOST_AUTO_TEST_CASE(test_explicit_construction)
{
	const char data[infohash_size] = {0};
	infohash_view ihv(data);	
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_copy_construction)
{
	const char data[infohash_size] = {0};
	infohash_view ihv(data);
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	infohash_view ihv2(ihv);
	BOOST_CHECK_EQUAL(ihv.data(), ihv2.data()); 
	BOOST_CHECK_EQUAL(ihv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_copy_assignement)
{
	const char data[infohash_size] = {0};
	infohash_view ihv(data);
	infohash_view ihv2;
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	BOOST_CHECK(ihv2.data() == nullptr);
	BOOST_CHECK_EQUAL(ihv2.empty(), true); 

	ihv2 = ihv;	
	BOOST_CHECK_EQUAL(ihv.data(), ihv2.data()); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	BOOST_CHECK_EQUAL(ihv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_move_construction)
{
	const char data[infohash_size] = {0};
	infohash_view ihv(data);
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	infohash_view ihv2(std::move(ihv));
	BOOST_CHECK_EQUAL(ihv.data(), ihv2.data()); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	BOOST_CHECK_EQUAL(ihv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_move_assignement)
{
	const char data[infohash_size] = {0};
	infohash_view ihv(data);
	infohash_view ihv2;
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	BOOST_CHECK(ihv2.data() == nullptr);
	BOOST_CHECK_EQUAL(ihv2.empty(), true); 

	ihv2 = std::move(ihv);	
	BOOST_CHECK_EQUAL(ihv.data(), ihv2.data()); 
	BOOST_CHECK_EQUAL(ihv.empty(), false); 
	BOOST_CHECK_EQUAL(ihv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_reset)
{
	const char data[infohash_size] = {0};
	infohash_view ihv;
	BOOST_CHECK(ihv.data() == nullptr);
	ihv.reset(data);
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	ihv.reset();
	BOOST_CHECK(ihv.data() == nullptr);
}

BOOST_AUTO_TEST_CASE(test_non_member_swap)
{
	const char data[infohash_size] = {0};
	const char data2[infohash_size] = {1};
	infohash_view ihv(data);
	infohash_view ihv2(data2);
	BOOST_CHECK_NE(ihv.data(), ihv2.data()); 
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv2.data(), static_cast<const char*>(data2)); 
	using std::swap;
	swap(ihv, ihv2);
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data2)); 
	BOOST_CHECK_EQUAL(ihv2.data(), static_cast<const char*>(data)); 
}

BOOST_AUTO_TEST_CASE(test_member_swap)
{
	const char data[infohash_size] = {0};
	const char data2[infohash_size] = {1};
	infohash_view ihv(data);
	infohash_view ihv2(data2);
	BOOST_CHECK_NE(ihv.data(), ihv2.data()); 
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(ihv2.data(), static_cast<const char*>(data2)); 
	ihv.swap(ihv2);
	BOOST_CHECK_EQUAL(ihv.data(), static_cast<const char*>(data2)); 
	BOOST_CHECK_EQUAL(ihv2.data(), static_cast<const char*>(data)); 
}

BOOST_AUTO_TEST_CASE(test_operator_square_brackets)
{
	auto sih = random_ih();
	infohash_view ihv(sih.data());
	bool same = true;
	for (size_t i = 0; i < ihv.size(); ++i)
	{
		if (ihv[i] != sih[i])
		{
			same = false;
			break;
		}
	}
	BOOST_CHECK_EQUAL(same, true); 
}

BOOST_AUTO_TEST_CASE(test_begin_end)
{
	auto sih = random_ih();
	infohash_view ihv(sih.data());
	BOOST_CHECK_EQUAL((std::equal(ihv.begin(), ihv.end(), sih.begin())), true); 
}

BOOST_AUTO_TEST_CASE(test_cbegin_cend)
{
	auto sih = random_ih();
	infohash_view ihv(sih.data());
	BOOST_CHECK_EQUAL((std::equal(ihv.cbegin(), ihv.cend(), sih.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_rbegin_rend)
{
	auto sih = random_ih();
	infohash_view ihv(sih.data());
	BOOST_CHECK_EQUAL((std::equal(ihv.rbegin(), ihv.rend(), sih.rbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_crbegin_crend)
{
	auto sih = random_ih();
	infohash_view ihv(sih.data());
	BOOST_CHECK_EQUAL((std::equal(ihv.crbegin(), ihv.crend(), sih.crbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_comparisson)
{
	auto sih = random_ih();
	std::array<char, 20> sih2;
	std::copy(sih.cbegin(), sih.cend(), sih2.begin());
	BOOST_CHECK(sih.data() != sih2.data()); 
	infohash_view ihv(sih.data());
	infohash_view ihv2(sih2.data());
	BOOST_CHECK_EQUAL(ihv, ihv2);
	ihv.reset();
	BOOST_CHECK_NE(ihv, ihv2);
	ihv.reset(sih.data());
	BOOST_CHECK_EQUAL(ihv, ihv2);
	ihv2.reset(nullptr);
	BOOST_CHECK_NE(ihv, ihv2);
	ihv2.reset(sih2.data());
	BOOST_CHECK_EQUAL(ihv, ihv2);
	ihv2.reset(ihv.data());
	BOOST_CHECK_EQUAL(ihv, ihv2);
}

BOOST_AUTO_TEST_CASE(test_comparisson_2)
{
	auto sih = random_ih();
	auto sih2 = random_ih();
	BOOST_CHECK_NE(sih, sih2);
	infohash_view ihv(sih.data());
	infohash_view ihv2(sih2.data());
	BOOST_CHECK_EQUAL((std::less<infohash_view>()(ihv, ihv2)), (std::less<std::string>()(sih, sih2))); 
	ihv.reset();
	BOOST_CHECK_LT(ihv, ihv2);
	ihv2.reset();
	BOOST_CHECK_GE(ihv, ihv2);
	BOOST_CHECK_EQUAL(ihv, ihv2);
	ihv.reset(sih.data());
	BOOST_CHECK_GT(ihv, ihv2);
}


BOOST_AUTO_TEST_CASE(test_comparisson_3)
{
	for (int i = 0; i < 50000; ++i)
	{
		auto sih1 = random_ih();
		auto sih2 = random_ih();

		BOOST_CHECK_EQUAL((std::equal(sih1.cbegin(), sih1.cend(), sih2.cbegin())), false); 

		infohash_view ih1(sih1.data());
		infohash_view ih2(sih2.data());
		
		BOOST_CHECK_EQUAL((std::equal(ih1.cbegin(), ih1.cend(), sih1.cbegin())), true); 
		BOOST_CHECK_EQUAL((std::equal(ih2.cbegin(), ih2.cend(), sih2.cbegin())), true);

		BOOST_CHECK_EQUAL((std::less<infohash_view>()(ih1, ih2)), (std::less<std::string>()(sih1, sih2))); 
		BOOST_CHECK_EQUAL((std::less_equal<infohash_view>()(ih1, ih2)), (std::less_equal<std::string>()(sih1, sih2))); 
		BOOST_CHECK_EQUAL((std::greater<infohash_view>()(ih1, ih2)), (std::greater<std::string>()(sih1, sih2))); 
		BOOST_CHECK_EQUAL((std::greater_equal<infohash_view>()(ih1, ih2)), 
				(std::greater_equal<std::string>()(sih1, sih2))); 
		BOOST_CHECK_EQUAL((std::equal_to<infohash_view>()(ih1, ih2)), (std::equal_to<std::string>()(sih1, sih2))); 
		BOOST_CHECK_EQUAL((std::not_equal_to<infohash_view>()(ih1, ih2)), 
				(std::not_equal_to<std::string>()(sih1, sih2))); 
	}	
}

BOOST_AUTO_TEST_CASE(test_streaming)
{
	for (int i = 0; i < 50000; ++i)
	{
		auto sih = random_ih();
		infohash_view ih(sih.data());

		std::ostringstream oss;
		oss << ih;

		std::string hex_sih;
		boost::algorithm::hex(sih.cbegin(), sih.cend(), std::back_inserter(hex_sih));

		auto hex_ih = oss.str();
		BOOST_CHECK_EQUAL(hex_ih.size(), infohash_size*2);
		BOOST_CHECK_EQUAL(oss.str(), hex_sih);
	}
}

BOOST_AUTO_TEST_CASE(test_hashing_stupid_way)
{
	enum { count = 50000 };
	std::vector<std::string> sih_array;
	sih_array.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		sih_array.emplace_back(random_ih());
	}
	std::unordered_set<infohash_view> ih_set;
	for (const auto& sih : sih_array)
	{
		ih_set.emplace(sih.data());
	}

	BOOST_CHECK_EQUAL(ih_set.size(), sih_array.size()); 

	for (const auto& sih : sih_array)
	{
		auto found = ih_set.find(infohash_view(sih.data()));
		BOOST_CHECK_EQUAL((found != ih_set.cend()), true); 
	}
}

BOOST_AUTO_TEST_SUITE_END()
