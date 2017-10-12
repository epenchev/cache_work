#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../xlog/log_target.h"
#include "../xlog/async_channel.h"
#include "../xlog/async_channel_impl.h"

using namespace xlog;
// The following are not the classic unit tests but ...

namespace
{

#define TEST_DIR "./lc_tests_aweqr83749_aada"
const std::string test_dir = TEST_DIR;

struct fixture
{
    fixture() noexcept { mkdir(test_dir.c_str(), S_IRWXU | S_IRGRP | S_IROTH); }
    ~fixture() noexcept
    {
        // Yeah, this is a dirty hack, but just for the unit tests in order
        // to keep the code short.
        auto r = system("rm -r " TEST_DIR);
        (void)r;
    }
};

#undef TEST_DIR

std::string read_all(const std::string& file_path)
{
    std::ifstream is(file_path, std::ifstream::binary);
    if (!is)
    {
        abort();
        return "";
    }

    is.seekg(0, is.end);
    size_t len = is.tellg();
    is.seekg(0, is.beg);

    std::string data(len, 'x');

    is.read(&data[0], data.size());
    if (is.gcount() != static_cast<std::streamsize>(len))
    {
        abort();
        return "";
    }

    return data;
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(log_channel_tests, fixture)

BOOST_AUTO_TEST_CASE(channel_wont_add_repeated_target)
{
    constexpr bool truncate     = true; // This doesn't matter for this test
    constexpr uint32_t hard_lim = 1024;
    constexpr uint32_t soft_lim = 768;
    constexpr target_id tid0(0);
    constexpr target_id tid1(1);
    const auto fpath0 = test_dir + "/fpath0";
    const auto fpath1 = test_dir + "/fpath1";
    const auto fpath2 = test_dir + "/fpath2";
    const auto fpath3 = test_dir + "/fpath3";

    auto chan = create_async_channel("test_chan", hard_lim, soft_lim);
    err_code_t err;
    BOOST_CHECK(chan.add_log_target(
        tid0, create_file_target(fpath0.c_str(), truncate, level::warn, err)));
    // Won't add a target with the same target_id
    BOOST_CHECK(!chan.add_log_target(
        tid0, create_file_target(fpath1.c_str(), truncate, level::warn, err)));
    // Will add target with different target_id
    BOOST_CHECK(chan.add_log_target(
        tid1, create_file_target(fpath1.c_str(), truncate, level::warn, err)));
    BOOST_CHECK(chan.add_explicit_log_target(
        tid0, create_file_target(fpath2.c_str(), truncate, level::warn, err)));
    // Won't add a target with the same target_id
    BOOST_CHECK(!chan.add_explicit_log_target(
        tid0, create_file_target(fpath3.c_str(), truncate, level::warn, err)));
    // Will add target with different target_id
    BOOST_CHECK(chan.add_explicit_log_target(
        tid1, create_file_target(fpath3.c_str(), truncate, level::warn, err)));
}

BOOST_AUTO_TEST_CASE(channel_wont_add_target_after_the_limit)
{
    constexpr bool truncate     = true; // This doesn't matter for this test
    constexpr uint32_t hard_lim = 1024;
    constexpr uint32_t soft_lim = 768;
    constexpr target_id tid0(0);
    constexpr target_id tid1(1);
    constexpr target_id tid2(2);
    constexpr target_id tid3(3);
    constexpr target_id tid4(4);
    const auto fpath0 = test_dir + "/fpath0";
    const auto fpath1 = test_dir + "/fpath1";
    const auto fpath2 = test_dir + "/fpath2";
    const auto fpath3 = test_dir + "/fpath3";
    const auto fpath4 = test_dir + "/fpath4";
    const auto fpath5 = test_dir + "/fpath5";
    const auto fpath6 = test_dir + "/fpath6";

    static_assert(detail::async_channel_impl::max_cnt_targets == 4,
                  "Change the test if this assert");
    static_assert(detail::async_channel_impl::max_cnt_expl_targets == 2,
                  "Change the test if this assert");

    auto chan = create_async_channel("test_chan", hard_lim, soft_lim);
    err_code_t err;
    BOOST_CHECK(chan.add_log_target(
        tid0, create_file_target(fpath0.c_str(), truncate, level::warn, err)));
    BOOST_CHECK(chan.add_log_target(
        tid1, create_file_target(fpath1.c_str(), truncate, level::warn, err)));
    BOOST_CHECK(chan.add_log_target(
        tid2, create_file_target(fpath2.c_str(), truncate, level::warn, err)));
    BOOST_CHECK(chan.add_log_target(
        tid3, create_file_target(fpath3.c_str(), truncate, level::warn, err)));
    // This should fail
    BOOST_CHECK(!chan.add_log_target(
        tid4, create_file_target(fpath4.c_str(), truncate, level::warn, err)));
    // Now for the explicit log targets
    BOOST_CHECK(chan.add_explicit_log_target(
        tid0, create_file_target(fpath4.c_str(), truncate, level::warn, err)));
    BOOST_CHECK(chan.add_explicit_log_target(
        tid1, create_file_target(fpath5.c_str(), truncate, level::warn, err)));
    BOOST_CHECK(!chan.add_explicit_log_target(
        tid2, create_file_target(fpath6.c_str(), truncate, level::warn, err)));
}

BOOST_AUTO_TEST_CASE(channel_max_log_level)
{
    // The channel max log level is determined by the
    // max log level of all of it's targets

    constexpr bool truncate     = true; // This doesn't matter for this test
    constexpr uint32_t hard_lim = 1024;
    constexpr uint32_t soft_lim = 768;
    constexpr target_id tid0(0);
    constexpr target_id tid1(1);
    constexpr target_id tid2(2);
    const auto fpath0 = test_dir + "/fpath0";
    const auto fpath1 = test_dir + "/fpath1";
    const auto fpath2 = test_dir + "/fpath2";
    const auto fpath3 = test_dir + "/fpath3";
    const auto fpath4 = test_dir + "/fpath4";

    auto chan = create_async_channel("test_chan", hard_lim, soft_lim);
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level(), to_number(level::off));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level_expl(), to_number(level::off));
    err_code_t err;
    chan.add_log_target(
        tid0, create_file_target(fpath0.c_str(), truncate, level::warn, err));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level(), to_number(level::warn));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level_expl(), to_number(level::off));
    chan.add_log_target(
        tid1, create_file_target(fpath1.c_str(), truncate, level::info, err));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level(), to_number(level::info));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level_expl(), to_number(level::off));
    // This target shouldn't change the max log level
    chan.add_log_target(
        tid2, create_file_target(fpath2.c_str(), truncate, level::error, err));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level(), to_number(level::info));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level_expl(), to_number(level::off));
    // Now check the explicit targets
    chan.add_explicit_log_target(
        tid0, create_file_target(fpath3.c_str(), truncate, level::info, err));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level(), to_number(level::info));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level_expl(),
                      to_number(level::info));
    // This target shouldn't change the max log level
    chan.add_explicit_log_target(
        tid1, create_file_target(fpath4.c_str(), truncate, level::fatal, err));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level(), to_number(level::info));
    BOOST_CHECK_EQUAL(chan.impl()->max_log_level_expl(),
                      to_number(level::info));
}

BOOST_AUTO_TEST_CASE(channel_log_to_target_by_level)
{
    constexpr bool truncate     = true; // This doesn't matter for this test
    constexpr bool force_queue  = true;
    constexpr uint32_t hard_lim = 1024;
    constexpr uint32_t soft_lim = 768;
    constexpr target_id tid0(0);
    constexpr target_id tid1(1);
    const auto fpath0      = test_dir + "/fpath0";
    const auto fpath1      = test_dir + "/fpath1";
    const auto fpath0_expl = test_dir + "/fpath0_expl";
    const auto fpath1_expl = test_dir + "/fpath1_expl";
    const auto ct          = time(nullptr);

    std::string fpath0_data;
    std::string fpath1_data;
    std::string fpath0_expl_data;
    std::string fpath1_expl_data;
    {
        err_code_t err;
        std::string data;

        auto chan = create_async_channel("test_chan", hard_lim, soft_lim);
        // Two normal and two explicit log targets
        BOOST_REQUIRE(chan.add_log_target(
            tid0,
            create_file_target(fpath0.c_str(), truncate, level::error, err)));
        BOOST_REQUIRE(chan.add_log_target(
            tid1,
            create_file_target(fpath1.c_str(), truncate, level::info, err)));
        BOOST_REQUIRE(chan.add_explicit_log_target(
            tid0, create_file_target(fpath0_expl.c_str(), truncate,
                                     level::fatal, err)));
        BOOST_REQUIRE(chan.add_explicit_log_target(
            tid1, create_file_target(fpath1_expl.c_str(), truncate, level::info,
                                     err)));

        chan.impl()->start();

        // Log to the non-explicit targets
        data = "This error should be logged in all non-explicit targets\n";
        chan.impl()->enque_log_msg(ct, level::error, xlog::invalid_target_id,
                                   data.data(), data.size(), force_queue);
        fpath0_data += data;
        fpath1_data += data;

        data = "This fatal should be logged in all non-explicit targets\n";
        chan.impl()->enque_log_msg(ct, level::fatal, xlog::invalid_target_id,
                                   data.data(), data.size(), force_queue);
        fpath0_data += data;
        fpath1_data += data;

        data = "This info should be logged in the non-explicit target #1\n";
        chan.impl()->enque_log_msg(ct, level::info, xlog::invalid_target_id,
                                   data.data(), data.size(), force_queue);
        fpath1_data += data;

        data = "This debug shouldn't be logged in any non-explicit target\n";
        chan.impl()->enque_log_msg(ct, level::debug, xlog::invalid_target_id,
                                   data.data(), data.size(), force_queue);

        // Log to the explicit targets
        data = "This fatal should be logged in the explicit target #0\n";
        chan.impl()->enque_log_msg(ct, level::fatal, tid0, data.data(),
                                   data.size(), force_queue);
        fpath0_expl_data += data;

        data = "This error shouldn't be logged in the explicit target #0\n";
        chan.impl()->enque_log_msg(ct, level::error, tid0, data.data(),
                                   data.size(), force_queue);

        data = "This error should be logged in the explicit target #1\n";
        chan.impl()->enque_log_msg(ct, level::error, tid1, data.data(),
                                   data.size(), force_queue);
        fpath1_expl_data += data;

        data = "This info should be logged in the explicit target #1\n";
        chan.impl()->enque_log_msg(ct, level::info, tid1, data.data(),
                                   data.size(), force_queue);
        fpath1_expl_data += data;

        data = "This debug shouldn't be logged in the explicit target #1\n";
        chan.impl()->enque_log_msg(ct, level::debug, tid1, data.data(),
                                   data.size(), force_queue);

        data =
            "This fatal shouldn't be logged in non existing explicit target\n";
        chan.impl()->enque_log_msg(ct, level::debug, xlog::target_id(42),
                                   data.data(), data.size(), force_queue);

        chan.impl()->stop(true /*wait flush*/);
    }

    // We need to skip the prefix record/row.
    // Quick and dirty way for it.
    auto check_all = [](const auto& orig, const auto& checked)
    {
        auto get_suffix = [](const auto& s, size_t suffix)
        {
            if (s.size() > suffix)
                return s.substr(s.size() - suffix);
            return s;
        };
        std::vector<std::string> tmp_checked, tmp_orig;
        boost::split(tmp_checked, checked, boost::is_any_of("\n"));
        boost::split(tmp_orig, orig, boost::is_any_of("\n"));
        BOOST_REQUIRE_EQUAL(tmp_checked.size(), tmp_orig.size());
        for (size_t i = 0; i < tmp_orig.size(); ++i)
        {
            const auto& c = tmp_checked[i];
            const auto& o = tmp_orig[i];
            BOOST_CHECK_EQUAL(get_suffix(c, o.size()), o);
        }
    };

    auto content = read_all(fpath0);
    check_all(fpath0_data, content);
    content = read_all(fpath1);
    check_all(fpath1_data, content);
    content = read_all(fpath0_expl);
    check_all(fpath0_expl_data, content);
    content = read_all(fpath1_expl);
    check_all(fpath1_expl_data, content);
}

BOOST_AUTO_TEST_SUITE_END()
