#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>

#include <boost/test/unit_test.hpp>

#include "common.h"

#include "../encode.h"
#include "../peer_id.h"

using namespace x3me::bt_utils;

BOOST_AUTO_TEST_SUITE(tests_peer_id) 

BOOST_AUTO_TEST_CASE(test_construction)
{
	auto spid = random_peer_id();
	BOOST_CHECK_EQUAL(spid.size(), peer_id_size);

	peer_id pid(spid.data());
	
	BOOST_CHECK_EQUAL((std::equal(pid.cbegin(), pid.cend(), spid.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_copy_construction)
{
	auto spid = random_peer_id();

	peer_id pid1(spid.data());
	peer_id pid2(pid1);
	
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), pid2.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_copy_assignement)
{
	auto spid = random_peer_id();

	peer_id pid1(spid.data());
	peer_id pid2;
	
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), pid2.cbegin())), false); 

	pid2 = pid1;
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), pid2.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_move_construction)
{
	auto spid = random_peer_id();

	peer_id pid1(spid.data());
	peer_id pid2(std::move(pid1));
	
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), pid2.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_move_assignement)
{
	auto spid = random_peer_id();

	peer_id pid1(spid.data());
	peer_id pid2;
	
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), pid2.cbegin())), false); 

	pid2 = std::move(pid1);
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), pid2.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_assign)
{
	auto spid = random_peer_id();

	peer_id pid;
	
	BOOST_CHECK_EQUAL((std::equal(pid.cbegin(), pid.cend(), spid.cbegin())), false); 

	pid.assign(spid.data());

	BOOST_CHECK_EQUAL((std::equal(pid.cbegin(), pid.cend(), spid.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_non_member_swap)
{
	auto spid1 = random_peer_id();
	auto spid2 = random_peer_id();

	BOOST_CHECK_EQUAL((std::equal(spid1.cbegin(), spid1.cend(), spid2.cbegin())), false); 

	peer_id pid1(spid1.data());
	peer_id pid2(spid2.data());
	
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), spid1.cbegin())), true); 
	BOOST_CHECK_EQUAL((std::equal(pid2.cbegin(), pid2.cend(), spid2.cbegin())), true);

	using std::swap;
	swap(pid1, pid2);

	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), spid2.cbegin())), true); 
	BOOST_CHECK_EQUAL((std::equal(pid2.cbegin(), pid2.cend(), spid1.cbegin())), true);
}

BOOST_AUTO_TEST_CASE(test_member_swap)
{
	auto spid1 = random_peer_id();
	auto spid2 = random_peer_id();

	BOOST_CHECK_EQUAL((std::equal(spid1.cbegin(), spid1.cend(), spid2.cbegin())), false); 

	peer_id pid1(spid1.data());
	peer_id pid2(spid2.data());
	
	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), spid1.cbegin())), true); 
	BOOST_CHECK_EQUAL((std::equal(pid2.cbegin(), pid2.cend(), spid2.cbegin())), true);

	pid1.swap(pid2);

	BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), spid2.cbegin())), true); 
	BOOST_CHECK_EQUAL((std::equal(pid2.cbegin(), pid2.cend(), spid1.cbegin())), true);
}

BOOST_AUTO_TEST_CASE(test_comparisson)
{
	for (int i = 0; i < 50000; ++i)
	{
		auto spid1 = random_peer_id();
		auto spid2 = random_peer_id();

		BOOST_CHECK_EQUAL((std::equal(spid1.cbegin(), spid1.cend(), spid2.cbegin())), false); 

		peer_id pid1(spid1.data());
		peer_id pid2(spid2.data());
		
		BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), spid1.cbegin())), true); 
		BOOST_CHECK_EQUAL((std::equal(pid2.cbegin(), pid2.cend(), spid2.cbegin())), true);

		BOOST_CHECK_EQUAL((std::less<peer_id>()(pid1, pid2)), (std::less<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::less_equal<peer_id>()(pid1, pid2)), (std::less_equal<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::greater<peer_id>()(pid1, pid2)), (std::greater<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::greater_equal<peer_id>()(pid1, pid2)), 
				(std::greater_equal<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::equal_to<peer_id>()(pid1, pid2)), (std::equal_to<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::not_equal_to<peer_id>()(pid1, pid2)), 
				(std::not_equal_to<std::string>()(spid1, spid2))); 
	}	
}

BOOST_AUTO_TEST_CASE(test_streaming)
{
	for (int i = 0; i < 50000; ++i)
	{
		auto spid = random_peer_id();
		peer_id pid(spid.data());

		std::ostringstream oss;
		oss << pid;

		std::string enc_spid;
		x3me::encode::encode_ascii_control_codes(spid, std::back_inserter(enc_spid));

		BOOST_CHECK_EQUAL(oss.str(), enc_spid);
	}
}

BOOST_AUTO_TEST_CASE(test_hashing_stupid_way)
{
	enum { count = 50000 };
	std::vector<std::string> spid_array;
	spid_array.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		spid_array.emplace_back(random_peer_id());
	}
	std::unordered_set<peer_id> pid_set;
	for (const auto& spid : spid_array)
	{
		pid_set.emplace(spid.data());
	}

	BOOST_CHECK_EQUAL(pid_set.size(), spid_array.size()); 

	for (const auto& spid : spid_array)
	{
		auto found = pid_set.find(peer_id(spid.data()));
		BOOST_CHECK_EQUAL((found != pid_set.cend()), true); 
	}
}

BOOST_AUTO_TEST_SUITE_END()
