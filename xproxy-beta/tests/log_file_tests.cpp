#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../xlog/log_file.h"

using xlog::detail::log_file;
using xlog::detail::hdr_data_t;
// The following are not the classic unit tests but ...

namespace
{

#define TEST_DIR "./lf_tests_0983749_aada"
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

BOOST_FIXTURE_TEST_SUITE(log_file_tests, fixture)

BOOST_AUTO_TEST_CASE(file_operations_on_invalid)
{
    char buff[64];

    log_file file;
    BOOST_CHECK(!file);
    BOOST_CHECK(!file.valid());
    {
        err_code_t err;
        BOOST_CHECK(!file.write(buff, sizeof(buff), err));
        BOOST_CHECK(err);
    }
    {
        err_code_t err;
        BOOST_CHECK(file.size(err) < 0);
        BOOST_CHECK(err);
    }
}

BOOST_AUTO_TEST_CASE(file_open_invalid_path)
{
    const auto fpath = "/blah/file_open_truncate";
    log_file file;
    BOOST_CHECK(!file);
    err_code_t err;
    BOOST_CHECK(!file.open(fpath, true /*truncate*/, err));
    BOOST_CHECK(err);
    BOOST_CHECK(!file.valid());
}

BOOST_AUTO_TEST_CASE(file_open_truncate)
{
    const auto fpath = test_dir + "/file_open_truncate";
    log_file file;
    BOOST_CHECK(!file);
    err_code_t err;
    BOOST_CHECK(file.open(fpath.c_str(), true /*truncate*/, err));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.valid());
    BOOST_CHECK(file.size(err) == 0);
    BOOST_CHECK(!err);
}

BOOST_AUTO_TEST_CASE(file_write)
{
    std::array<char, 128> buff;
    std::iota(buff.begin(), buff.end(), 0);

    const auto fpath = test_dir + "/file_write";
    log_file file;
    BOOST_CHECK(!file);
    err_code_t err;
    BOOST_CHECK(file.open(fpath.c_str(), true /*truncate*/, err));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.write(buff.data(), buff.size() / 2, err));
    BOOST_CHECK(!err);
    BOOST_CHECK(
        file.write(buff.data() + (buff.size() / 2), buff.size() / 2, err));
    BOOST_CHECK(!err);

    const auto content = read_all(fpath);
    BOOST_REQUIRE_EQUAL(content.size(), buff.size());
    BOOST_CHECK(std::equal(content.cbegin(), content.cend(), buff.cbegin()));
}

BOOST_AUTO_TEST_CASE(file_open_no_truncate)
{
    std::array<char, 128> buff;
    std::iota(buff.begin(), buff.end(), 0);

    const auto fpath = test_dir + "/file_write";

    log_file file;
    BOOST_CHECK(!file);
    err_code_t err;
    BOOST_CHECK(file.open(fpath.c_str(), true /*truncate*/, err));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.write(buff.data(), buff.size() / 2, err));
    BOOST_CHECK(!err);
    BOOST_CHECK_EQUAL(file.size(err), (buff.size() / 2));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.close(err));
    BOOST_CHECK(!err);

    BOOST_CHECK(file.open(fpath.c_str(), false /*truncate*/, err));
    BOOST_CHECK(!err);
    BOOST_CHECK_EQUAL(file.size(err), (buff.size() / 2));
    BOOST_CHECK(!err);
}

BOOST_AUTO_TEST_CASE(file_write_no_truncate)
{
    std::array<char, 128> buff;
    std::iota(buff.begin(), buff.end(), 0);

    const auto fpath = test_dir + "/file_write";

    log_file file;
    BOOST_CHECK(!file);
    err_code_t err;
    BOOST_CHECK(file.open(fpath.c_str(), true /*truncate*/, err));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.write(buff.data(), buff.size() / 2, err));
    BOOST_CHECK(!err);
    BOOST_CHECK_EQUAL(file.size(err), (buff.size() / 2));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.close(err));
    BOOST_CHECK(!err);

    BOOST_CHECK(file.open(fpath.c_str(), false /*truncate*/, err));
    BOOST_CHECK(!err);
    BOOST_CHECK_EQUAL(file.size(err), (buff.size() / 2));
    BOOST_CHECK(!err);

    BOOST_CHECK(
        file.write(buff.data() + (buff.size() / 2), buff.size() / 2, err));
    BOOST_CHECK(!err);
    BOOST_CHECK_EQUAL(file.size(err), buff.size());
    BOOST_CHECK(!err);

    const auto content = read_all(fpath);
    BOOST_REQUIRE_EQUAL(content.size(), buff.size());
    BOOST_CHECK(std::equal(content.cbegin(), content.cend(), buff.cbegin()));
}

BOOST_AUTO_TEST_CASE(file_write_hdr_data)
{
    std::string expected;

    std::array<char, 2048> buff1;
    std::fill(buff1.begin(), buff1.end(), 'A');
    std::array<char, 4096> buff2;
    std::fill(buff2.begin(), buff2.end(), 'B');

    hdr_data_t hd;

    const auto fpath = test_dir + "/file_write_hdr_data";

    log_file file;
    BOOST_CHECK(!file);
    err_code_t err;
    BOOST_CHECK(file.open(fpath.c_str(), true /*truncate*/, err));
    BOOST_CHECK(!err);

    hd[0].iov_base = buff1.data();
    hd[0].iov_len  = 1456;
    hd[1].iov_base = buff2.data();
    hd[1].iov_len = 3345;
    expected.append((const char*)hd[0].iov_base, hd[0].iov_len);
    expected.append((const char*)hd[1].iov_base, hd[1].iov_len);
    BOOST_CHECK(file.write(hd, err));
    BOOST_CHECK(!err);

    BOOST_CHECK(file.close(err));
    BOOST_CHECK(!err);
    BOOST_CHECK(file.open(fpath.c_str(), false /*truncate*/, err));
    BOOST_CHECK(!err);

    hd[0].iov_base = buff1.data();
    hd[0].iov_len  = 2047;
    hd[1].iov_base = buff2.data();
    hd[1].iov_len = 1;
    expected.append((const char*)hd[0].iov_base, hd[0].iov_len);
    expected.append((const char*)hd[1].iov_base, hd[1].iov_len);
    BOOST_CHECK(file.write(hd, err));
    BOOST_CHECK(!err);

    hd[0].iov_base = buff1.data();
    hd[0].iov_len  = 1;
    hd[1].iov_base = buff2.data();
    hd[1].iov_len = 4095;
    expected.append((const char*)hd[0].iov_base, hd[0].iov_len);
    expected.append((const char*)hd[1].iov_base, hd[1].iov_len);
    BOOST_CHECK(file.write(hd, err));
    BOOST_CHECK(!err);

    const auto content = read_all(fpath);
    BOOST_REQUIRE_EQUAL(content.size(), expected.size());
    BOOST_CHECK_EQUAL(content, expected);
}

BOOST_AUTO_TEST_CASE(file_write_remove_range)
{
    // Do the test only if the kernel version allows it
    using namespace x3me::sys_utils;
    kern_ver kv;
    if (kernel_version(kv) &&
        ((kv.version_ < 3) || ((kv.version_ == 3) && (kv.major_rev_ <= 15))))
    {
        BOOST_TEST_MESSAGE(
            "Skipping test file_write_remove_range due to the kernel version");
        return;
    }

    std::array<char, 4096> buff1;
    std::fill(buff1.begin(), buff1.end(), 'A');
    std::array<char, 4096> buff2;
    std::fill(buff2.begin(), buff2.end(), 'B');

    const auto fpath = test_dir + "/file_write";

    err_code_t err;
    log_file file;
    BOOST_CHECK(file.open(fpath.c_str(), true /*truncate*/, err));
    file.close(err);
    // We are going to call remove_range thus the file needs to be opened
    // in append mode.
    BOOST_CHECK(file.open(fpath.c_str(), false /*append*/, err));
    BOOST_CHECK(!err);
    // The removed range must be module of the block_size
    BOOST_REQUIRE_EQUAL(buff1.size(), file.block_size(err));
    BOOST_CHECK(!err);
    BOOST_REQUIRE_EQUAL(buff1.size(), buff2.size());
    BOOST_CHECK(file.write(buff1.data(), buff1.size(), err));
    BOOST_CHECK(file.write(buff2.data(), buff2.size(), err));
    {
        const auto content = read_all(fpath);
        BOOST_REQUIRE_EQUAL(content.size(), buff1.size() + buff2.size());
        BOOST_CHECK(std::equal(buff1.cbegin(), buff1.cend(), content.cbegin()));
        BOOST_CHECK(std::equal(buff2.cbegin(), buff2.cend(),
                               content.cbegin() + buff1.size()));
    }
    // Remove the first half or the file
    BOOST_CHECK(file.remove_range(0, buff1.size(), err));
    BOOST_CHECK_MESSAGE(!err, err.message());
    {
        const auto content = read_all(fpath);
        BOOST_REQUIRE_EQUAL(content.size(), buff2.size());
        BOOST_CHECK(std::equal(buff2.cbegin(), buff2.cend(), content.cbegin()));
    }
    // Write the first half as second half again
    BOOST_CHECK(file.write(buff1.data(), buff1.size(), err));
    {
        const auto content = read_all(fpath);
        BOOST_REQUIRE_EQUAL(content.size(), buff2.size() + buff1.size());
        BOOST_CHECK(std::equal(buff2.cbegin(), buff2.cend(), content.cbegin()));
        BOOST_CHECK(std::equal(buff1.cbegin(), buff1.cend(),
                               content.cbegin() + buff2.size()));
    }
    // Remove the first half or the file
    BOOST_CHECK(file.remove_range(0, buff2.size(), err));
    BOOST_CHECK_MESSAGE(!err, err.message());
    {
        const auto content = read_all(fpath);
        BOOST_REQUIRE_EQUAL(content.size(), buff2.size());
        BOOST_CHECK(std::equal(buff1.cbegin(), buff1.cend(), content.cbegin()));
    }
}

BOOST_AUTO_TEST_SUITE_END()
