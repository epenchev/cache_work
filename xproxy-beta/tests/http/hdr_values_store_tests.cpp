#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../http/hdr_values_store.h"
#include "../http/hdr_values_store.ipp"

using namespace http::detail;

BOOST_AUTO_TEST_SUITE(hdr_values_store_tests)

BOOST_AUTO_TEST_CASE(default_construct)
{
    hdr_values_store<16> store;
    const auto key = store.current_key();
    BOOST_CHECK(key.key_.empty());
    BOOST_CHECK(key.full_);
    const auto val = store.current_value_view();
    BOOST_CHECK(val.empty());
}

BOOST_AUTO_TEST_CASE(key_fit_in_buffer)
{
    // Smaller than the buffer size
    const string_view_t key_str{"TestKey"};
    // Equal to the buffer size
    const string_view_t key_str1{"TestKey1"};

    hdr_values_store<8> store;
    store.start_key();
    BOOST_REQUIRE(store.append_key(key_str.data(), key_str.size()));
    auto key = store.current_key();
    BOOST_CHECK_EQUAL(key.key_, key_str);
    BOOST_CHECK(key.full_);
    store.start_key();
    BOOST_REQUIRE(store.append_key(key_str1.data(), key_str1.size()));
    key = store.current_key();
    BOOST_CHECK_EQUAL(key.key_, key_str1);
    BOOST_CHECK(key.full_);
}

BOOST_AUTO_TEST_CASE(key_dont_fit_in_buffer)
{
    // Smaller than the buffer size
    const std::string key_part0{"TestKey"};
    // This part will overflow the buffer after the first letter
    const std::string key_part1{"BestKey"};

    hdr_values_store<8> store;
    store.start_key();
    BOOST_REQUIRE(store.append_key(key_part0.data(), key_part0.size()));
    BOOST_REQUIRE(store.append_key(key_part1.data(), key_part1.size()));
    const auto key = store.current_key();
    BOOST_CHECK_EQUAL(key.key_, key_part0 + key_part1[0]);
    BOOST_CHECK(!key.full_);
}

BOOST_AUTO_TEST_CASE(key_bigger_than_max_allowed)
{
    // Smaller than the buffer size
    const std::string key_part0{"TestKey0"};

    hdr_values_store<8> store;
    store.start_key();
    BOOST_REQUIRE(store.append_key(key_part0.data(), key_part0.size()));

    const std::vector<char> v(decltype(store)::max_key_len - key_part0.size(),
                              'A');
    BOOST_CHECK(store.append_key(v.data(), v.size()));
    // The next append_key fails, because it goes over the limit
    BOOST_CHECK(!store.append_key(v.data(), 1));

    const auto key = store.current_key();
    BOOST_CHECK_EQUAL(key.key_, key_part0);
    BOOST_CHECK(!key.full_);
}

BOOST_AUTO_TEST_CASE(values_fit_in_store)
{
    hdr_values_store<8> store;

    // Smaller than the buffer size
    const std::string val0{"TestVal"};
    const std::string val1(decltype(store)::max_store_size - val0.size(), 'A');

    // This is much smaller than the buffer
    BOOST_REQUIRE(store.append_value(val0.data(), val0.size()));
    auto v = store.current_value_view();
    BOOST_CHECK_EQUAL((string_view_t{v.data(), v.size()}), val0);
    store.commit_current_value();
    // This should succeed, appending up to the end of the store
    BOOST_REQUIRE(store.append_value(val1.data(), val1.size()));
    v = store.current_value_view();
    BOOST_CHECK_EQUAL((string_view_t{v.data(), v.size()}), val1);
    store.commit_current_value();
}

BOOST_AUTO_TEST_CASE(values_dont_fit_in_store)
{
    hdr_values_store<8> store;

    // Smaller than the buffer size
    const std::string val0{"TestVal"};
    const std::string val1(decltype(store)::max_store_size + 1 - val0.size(),
                           'A');

    // This is much smaller than the buffer
    BOOST_REQUIRE(store.append_value(val0.data(), val0.size()));
    auto v = store.current_value_view();
    BOOST_CHECK_EQUAL((string_view_t{v.data(), v.size()}), val0);
    store.commit_current_value();
    // This shouldn't succeed because we go one past the end.
    BOOST_REQUIRE(!store.append_value(val1.data(), val1.size()));
    v = store.current_value_view();
    BOOST_CHECK(v.empty());
}

BOOST_AUTO_TEST_CASE(mulitple_commit_remove_current_value)
{
    hdr_values_store<8> store;

    // All values would fit in the buffer size
    const string_view_t val0{"TestVal000000000000000000000000000000000"};
    const string_view_t val1{"TestVal011111111111111111111111111111111"};
    const string_view_t val2{"TestVal022222222222222222222222222222222"};

    BOOST_REQUIRE(store.append_value(val0.data(), val0.size()));
    auto vp = store.current_value_pos();
    BOOST_CHECK_EQUAL(store.value_pos_to_view(vp), val0);
    store.commit_current_value();
    // After commit the value is still there
    BOOST_CHECK_EQUAL(store.value_pos_to_view(vp), val0);
    // Add the next value
    BOOST_REQUIRE(store.append_value(val1.data(), val1.size()));
    auto vp1 = store.current_value_pos();
    BOOST_CHECK_EQUAL(store.value_pos_to_view(vp1), val1);
    store.remove_current_value();

    // Add the next value
    BOOST_REQUIRE(store.append_value(val2.data(), val2.size()));
    auto vp2 = store.current_value_pos();
    BOOST_CHECK_EQUAL(store.value_pos_to_view(vp2), val2);
    store.commit_current_value();
    // After commit the value is still there
    BOOST_CHECK_EQUAL(store.value_pos_to_view(vp2), val2);
    // And the first value is still the same
    BOOST_CHECK_EQUAL(store.value_pos_to_view(vp), val0);
}

BOOST_AUTO_TEST_CASE(move_constructor)
{
    const string_view_t key{"TestKey"};
    const string_view_t val0{"TestVal000000000000000000000000000000000"};
    const string_view_t val1{"TestVal011111111111111111111111111111111"};

    hdr_values_store<8> store;

    BOOST_REQUIRE(store.append_key(key.data(), key.size()));
    auto k = store.current_key();
    BOOST_CHECK_EQUAL(k.key_, key);
    BOOST_CHECK(k.full_);

    BOOST_REQUIRE(store.append_value(val0.data(), val0.size()));
    const auto pos = store.current_value_pos();
    BOOST_CHECK_EQUAL(store.current_value_view(), val0);
    store.commit_current_value();
    BOOST_REQUIRE(store.append_value(val1.data(), val1.size()));
    BOOST_CHECK_EQUAL(store.current_value_view(), val1);
    // Note that the second value remains uncommitted

    // Now after the move everything is the same in the 'moved-to' object.
    // The 'moved-from' one is empty
    decltype(store) store2(std::move(store));

    k = store2.current_key();
    BOOST_CHECK_EQUAL(k.key_, key);
    BOOST_CHECK(k.full_);
    // The stored position also remains valid
    BOOST_CHECK_EQUAL(store2.value_pos_to_view(pos), val0);
    BOOST_CHECK_EQUAL(store2.current_value_view(), val1);

    BOOST_CHECK(store.current_key().key_.empty());
    BOOST_CHECK(store.current_value_pos().empty());
    BOOST_CHECK(store.current_value_view().empty());
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    const string_view_t key{"TestKey"};
    const string_view_t val0{"TestVal000000000000000000000000000000000"};
    const string_view_t val1{"TestVal011111111111111111111111111111111"};

    hdr_values_store<8> store, store2;

    BOOST_REQUIRE(store.append_key(key.data(), key.size()));
    auto k = store.current_key();
    BOOST_CHECK_EQUAL(k.key_, key);
    BOOST_CHECK(k.full_);

    BOOST_REQUIRE(store.append_value(val0.data(), val0.size()));
    const auto pos = store.current_value_pos();
    BOOST_CHECK_EQUAL(store.current_value_view(), val0);
    store.commit_current_value();
    BOOST_REQUIRE(store.append_value(val1.data(), val1.size()));
    BOOST_CHECK_EQUAL(store.current_value_view(), val1);
    // Note that the second value remains uncommitted

    // Now after the move everything is the same in the 'moved-to' object.
    // The 'moved-from' one is empty
    store2 = std::move(store);

    k = store2.current_key();
    BOOST_CHECK_EQUAL(k.key_, key);
    BOOST_CHECK(k.full_);
    // The stored position also remains valid
    BOOST_CHECK_EQUAL(store2.value_pos_to_view(pos), val0);
    BOOST_CHECK_EQUAL(store2.current_value_view(), val1);

    BOOST_CHECK(store.current_key().key_.empty());
    BOOST_CHECK(store.current_value_pos().empty());
    BOOST_CHECK(store.current_value_view().empty());
}

BOOST_AUTO_TEST_SUITE_END()
