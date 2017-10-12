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

BOOST_AUTO_TEST_SUITE(tests_peer_id_view) 

BOOST_AUTO_TEST_CASE(test_default_construction)
{
	peer_id_view pidv;
	BOOST_CHECK(pidv.data() == nullptr);
	BOOST_CHECK_EQUAL(pidv.empty(), true); 
}

BOOST_AUTO_TEST_CASE(test_explicit_construction)
{
	const char data[peer_id_size] = {0};
	peer_id_view pidv(data);	
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_copy_construction)
{
	const char data[peer_id_size] = {0};
	peer_id_view pidv(data);
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	peer_id_view pidv2(pidv);
	BOOST_CHECK_EQUAL(pidv.data(), pidv2.data()); 
	BOOST_CHECK_EQUAL(pidv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_copy_assignement)
{
	const char data[peer_id_size] = {0};
	peer_id_view pidv(data);
	peer_id_view pidv2;
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	BOOST_CHECK(pidv2.data() == nullptr);
	BOOST_CHECK_EQUAL(pidv2.empty(), true); 

	pidv2 = pidv;	
	BOOST_CHECK_EQUAL(pidv.data(), pidv2.data()); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	BOOST_CHECK_EQUAL(pidv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_move_construction)
{
	const char data[peer_id_size] = {0};
	peer_id_view pidv(data);
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	peer_id_view pidv2(std::move(pidv));
	BOOST_CHECK_EQUAL(pidv.data(), pidv2.data()); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	BOOST_CHECK_EQUAL(pidv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_move_assignement)
{
	const char data[peer_id_size] = {0};
	peer_id_view pidv(data);
	peer_id_view pidv2;
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	BOOST_CHECK(pidv2.data() == nullptr);
	BOOST_CHECK_EQUAL(pidv2.empty(), true); 

	pidv2 = std::move(pidv);	
	BOOST_CHECK_EQUAL(pidv.data(), pidv2.data()); 
	BOOST_CHECK_EQUAL(pidv.empty(), false); 
	BOOST_CHECK_EQUAL(pidv2.empty(), false); 
}

BOOST_AUTO_TEST_CASE(test_reset)
{
	const char data[peer_id_size] = {0};
	peer_id_view pidv;
	BOOST_CHECK(pidv.data() == nullptr);
	pidv.reset(data);
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	pidv.reset();
	BOOST_CHECK(pidv.data() == nullptr);
}

BOOST_AUTO_TEST_CASE(test_non_member_swap)
{
	const char data[peer_id_size] = {0};
	const char data2[peer_id_size] = {1};
	peer_id_view pidv(data);
	peer_id_view pidv2(data2);
	BOOST_CHECK_NE(pidv.data(), pidv2.data()); 
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv2.data(), static_cast<const char*>(data2)); 
	using std::swap;
	swap(pidv, pidv2);
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data2)); 
	BOOST_CHECK_EQUAL(pidv2.data(), static_cast<const char*>(data)); 
}

BOOST_AUTO_TEST_CASE(test_member_swap)
{
	const char data[peer_id_size] = {0};
	const char data2[peer_id_size] = {1};
	peer_id_view pidv(data);
	peer_id_view pidv2(data2);
	BOOST_CHECK_NE(pidv.data(), pidv2.data()); 
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data)); 
	BOOST_CHECK_EQUAL(pidv2.data(), static_cast<const char*>(data2)); 
	pidv.swap(pidv2);
	BOOST_CHECK_EQUAL(pidv.data(), static_cast<const char*>(data2)); 
	BOOST_CHECK_EQUAL(pidv2.data(), static_cast<const char*>(data)); 
}

BOOST_AUTO_TEST_CASE(test_operator_square_brackets)
{
	auto spid = random_peer_id();
	peer_id_view pidv(spid.data());
	bool same = true;
	for (size_t i = 0; i < pidv.size(); ++i)
	{
		if (pidv[i] != spid[i])
		{
			same = false;
			break;
		}
	}
	BOOST_CHECK_EQUAL(same, true); 
}

BOOST_AUTO_TEST_CASE(test_begin_end)
{
	auto spid = random_peer_id();
	peer_id_view pidv(spid.data());
	BOOST_CHECK_EQUAL((std::equal(pidv.begin(), pidv.end(), spid.begin())), true); 
}

BOOST_AUTO_TEST_CASE(test_cbegin_cend)
{
	auto spid = random_peer_id();
	peer_id_view pidv(spid.data());
	BOOST_CHECK_EQUAL((std::equal(pidv.cbegin(), pidv.cend(), spid.cbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_rbegin_rend)
{
	auto spid = random_peer_id();
	peer_id_view pidv(spid.data());
	BOOST_CHECK_EQUAL((std::equal(pidv.rbegin(), pidv.rend(), spid.rbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_crbegin_crend)
{
	auto spid = random_peer_id();
	peer_id_view pidv(spid.data());
	BOOST_CHECK_EQUAL((std::equal(pidv.crbegin(), pidv.crend(), spid.crbegin())), true); 
}

BOOST_AUTO_TEST_CASE(test_comparisson)
{
	auto spid = random_peer_id();
	std::array<char, 20> spid2;
	std::copy(spid.cbegin(), spid.cend(), spid2.begin());
	BOOST_CHECK(spid.data() != spid2.data()); 
	peer_id_view pidv(spid.data());
	peer_id_view pidv2(spid2.data());
	BOOST_CHECK_EQUAL(pidv, pidv2);
	pidv.reset();
	BOOST_CHECK_NE(pidv, pidv2);
	pidv.reset(spid.data());
	BOOST_CHECK_EQUAL(pidv, pidv2);
	pidv2.reset(nullptr);
	BOOST_CHECK_NE(pidv, pidv2);
	pidv2.reset(spid2.data());
	BOOST_CHECK_EQUAL(pidv, pidv2);
	pidv2.reset(pidv.data());
	BOOST_CHECK_EQUAL(pidv, pidv2);
}

BOOST_AUTO_TEST_CASE(test_comparisson_2)
{
	auto spid = random_peer_id();
	auto spid2 = random_peer_id();
	BOOST_CHECK_NE(spid, spid2);
	peer_id_view pidv(spid.data());
	peer_id_view pidv2(spid2.data());
	BOOST_CHECK_EQUAL((std::less<peer_id_view>()(pidv, pidv2)), (std::less<std::string>()(spid, spid2))); 
	pidv.reset();
	BOOST_CHECK_LT(pidv, pidv2);
	pidv2.reset();
	BOOST_CHECK_GE(pidv, pidv2);
	BOOST_CHECK_EQUAL(pidv, pidv2);
	pidv.reset(spid.data());
	BOOST_CHECK_GT(pidv, pidv2);
}


BOOST_AUTO_TEST_CASE(test_comparisson_3)
{
	for (int i = 0; i < 50000; ++i)
	{
		auto spid1 = random_peer_id();
		auto spid2 = random_peer_id();

		BOOST_CHECK_EQUAL((std::equal(spid1.cbegin(), spid1.cend(), spid2.cbegin())), false); 

		peer_id_view pid1(spid1.data());
		peer_id_view pid2(spid2.data());
		
		BOOST_CHECK_EQUAL((std::equal(pid1.cbegin(), pid1.cend(), spid1.cbegin())), true); 
		BOOST_CHECK_EQUAL((std::equal(pid2.cbegin(), pid2.cend(), spid2.cbegin())), true);

		BOOST_CHECK_EQUAL((std::less<peer_id_view>()(pid1, pid2)), (std::less<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::less_equal<peer_id_view>()(pid1, pid2)), (std::less_equal<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::greater<peer_id_view>()(pid1, pid2)), (std::greater<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::greater_equal<peer_id_view>()(pid1, pid2)), 
				(std::greater_equal<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::equal_to<peer_id_view>()(pid1, pid2)), (std::equal_to<std::string>()(spid1, spid2))); 
		BOOST_CHECK_EQUAL((std::not_equal_to<peer_id_view>()(pid1, pid2)), 
				(std::not_equal_to<std::string>()(spid1, spid2))); 
	}	
}

BOOST_AUTO_TEST_CASE(test_streaming)
{
	for (int i = 0; i < 50000; ++i)
	{
		auto spid = random_peer_id();
		peer_id_view pid(spid.data());

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
	std::unordered_set<peer_id_view> pid_set;
	for (const auto& spid : spid_array)
	{
		pid_set.emplace(spid.data());
	}

	BOOST_CHECK_EQUAL(pid_set.size(), spid_array.size()); 

	for (const auto& spid : spid_array)
	{
		auto found = pid_set.find(peer_id_view(spid.data()));
		BOOST_CHECK_EQUAL((found != pid_set.cend()), true); 
	}
}

BOOST_AUTO_TEST_SUITE_END()
