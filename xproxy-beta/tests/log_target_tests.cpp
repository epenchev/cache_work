#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../xlog/log_target.h"
#include "../xlog/log_target_impl.h"
#include "../xlog/log_file.h"

using namespace xlog;
// The following are not the classic unit tests but ...

namespace
{

#define TEST_DIR "./lt_tests_0983749_aada"
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

// Patch function needed after changes in the targets API.
void write_data(log_target& lt, const char* data, size_t size) noexcept
{
    BOOST_ASSERT(size > 1);
    const auto half_size = size / 2;
    detail::hdr_data_t hd;
    hd[0].iov_base = const_cast<char*>(data);
    hd[0].iov_len  = half_size;
    hd[1].iov_base = const_cast<char*>(data + half_size);
    hd[1].iov_len = size - half_size;
    lt.impl()->write(hd);
    // This was the API before
    // lt.impl()->write(data, size);
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(log_target_tests, fixture)

BOOST_AUTO_TEST_CASE(file_log_target)
{
    const auto fpath    = test_dir + "/file_log_target";
    const bool truncate = true;

    const std::string msg0 = "Log 0 message\n";
    const std::string msg1 = "Log 1 message\n";

    err_code_t err;
    {
        auto lt = create_file_target(fpath.c_str(), truncate, level::warn, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::warn));

        write_data(lt, msg0.data(), msg0.size());
        write_data(lt, msg1.data(), msg1.size());
    }
    {
        auto lt =
            create_file_target(fpath.c_str(), !truncate, level::warn, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::warn));

        write_data(lt, msg0.data(), msg0.size());
        write_data(lt, msg1.data(), msg1.size());
    }

    auto content = read_all(fpath);
    BOOST_CHECK_EQUAL(content, (msg0 + msg1 + msg0 + msg1));
}

BOOST_AUTO_TEST_CASE(file_rotate_log_target)
{
    const auto fpath              = test_dir + "/file_rotate_log_target";
    constexpr uint32_t block_size = 4096;

    const bool truncate = true;
    const std::string msg0(block_size * 4, 'A');
    const std::string msg1(block_size * 4, 'B');
    const uint64_t max_file_size = block_size * 4;
    constexpr uint32_t num0      = 17;
    constexpr uint32_t num1      = 12;

    auto on_pre_rotate = [](const std::string& curr)
    { // Return new file path
        return curr + '1';
    };

    {
        err_code_t err;
        auto lt =
            create_file_rotate_target(fpath.c_str(), truncate, max_file_size,
                                      level::error, on_pre_rotate, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::error));
        write_data(lt, msg0.data(), block_size);
        write_data(lt, msg0.data() + block_size, 2 * block_size);
        // Here it should get rotated and (2 * block_size) should get removed
        write_data(lt, msg0.data() + block_size, block_size + num0);
        // This record should go to the new log file
        write_data(lt, msg1.data(), block_size);
    }
    {
        auto content = read_all(fpath);
        std::string expected((4 * block_size) + num0, msg0[0]);
        BOOST_CHECK_EQUAL(content, expected);
        content = read_all(fpath + '1');
        expected.assign(msg1.data(), block_size);
        BOOST_CHECK_EQUAL(content, expected);
    }
    // Open the file again and write_data i.e. imitate application restart
    {
        err_code_t err;
        const auto new_path = fpath + '1';

        auto lt = create_file_rotate_target(new_path.c_str(), !truncate,
                                            max_file_size, level::warn,
                                            on_pre_rotate, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::warn));
        write_data(lt, msg0.data(), block_size);
        write_data(lt, msg0.data(), block_size);
        write_data(lt, msg1.data(), block_size - 2);
        write_data(lt, msg1.data() + block_size - 2, 2);
        // Here it should get rotated and (2 * block_size) should get removed
        write_data(lt, msg0.data(), num1);
    }
    {
        auto content = read_all(fpath + '1');
        std::string expected;
        expected.append(msg1.data(), block_size);
        expected.append(msg0.data(), block_size);
        expected.append(msg0.data(), block_size);
        expected.append(msg1.data(), block_size - 2);
        expected.append(msg1.data() + block_size - 2, 2);
        BOOST_CHECK_EQUAL(content, expected);
        content = read_all(fpath + "11");
        expected.assign(msg0.data(), num1);
        BOOST_CHECK_EQUAL(content, expected);
    }
    // Open the file once again and write_data i.e. imitate application restart
    {
        err_code_t err;
        const auto new_path = fpath + "11";

        auto lt = create_file_rotate_target(new_path.c_str(), !truncate,
                                            max_file_size, level::info,
                                            on_pre_rotate, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::info));
        write_data(lt, msg1.data(), msg1.size());
        // The file is rotated and new file should have been created.
        write_data(lt, "AA", 2);
    }
    {
        auto content = read_all(fpath + "11");
        std::string expected;
        expected.append(msg0.data(), num1);
        expected.append(msg1.data(), msg1.size());
        BOOST_CHECK_EQUAL(content, expected);
        content = read_all(fpath + "111");
        BOOST_CHECK_EQUAL(content, "AA");
    }
}

BOOST_AUTO_TEST_CASE(file_sliding_log_target)
{
    // Do the test only if the kernel version allows it
    using namespace x3me::sys_utils;
    kern_ver kv;
    if (kernel_version(kv) &&
        ((kv.version_ < 3) || ((kv.version_ == 3) && (kv.major_rev_ <= 15))))
    {
        BOOST_TEST_MESSAGE(
            "Skipping test file_sliding_log_target due to the kernel version");
        return;
    }

    const auto fpath    = test_dir + "/file_sliding_log_target";
    uint32_t block_size = 0;

    {
        // Just to truncate the file and get the block size
        const bool truncate = true;
        err_code_t err;
        detail::log_file file;
        BOOST_CHECK(file.open(fpath.c_str(), truncate, err));
        // The removed range must be module of the block_size
        block_size = file.block_size(err);
        BOOST_REQUIRE(block_size > 0);
        BOOST_REQUIRE(block_size == 4096);
    }

    const std::string msg0(block_size * 4, 'A');
    const std::string msg1(block_size * 4, 'B');
    const uint64_t max_file_size  = block_size * 4;
    const uint32_t size_tolerance = block_size;
    constexpr uint32_t num0       = 17;
    constexpr uint32_t num1       = 12;

    {
        err_code_t err;
        auto lt = create_file_sliding_target(fpath.c_str(), max_file_size,
                                             size_tolerance, level::error, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::error));
        write_data(lt, msg0.data(), block_size);
        write_data(lt, msg0.data() + block_size, 2 * block_size);
        // Here it should get slided and (2 * block_size) should get removed
        write_data(lt, msg0.data() + block_size, block_size + num0);
    }
    {
        auto content = read_all(fpath);
        BOOST_CHECK_EQUAL(content,
                          string_view_t(msg0.data(), (2 * block_size) + num0));
    }
    // Open the file again and write_data i.e. imitate application restart
    {
        err_code_t err;
        auto lt = create_file_sliding_target(fpath.c_str(), max_file_size,
                                             size_tolerance, level::error, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::error));
        // Here it should get slided and (2 * block_size) should get removed
        write_data(lt, msg1.data(), (2 * block_size) + num1);
    }
    {
        auto content = read_all(fpath);
        std::string expected(msg0.data(), num0);
        expected.append(msg1.data(), (2 * block_size) + num1);
        BOOST_CHECK_EQUAL(content, expected);
    }
    // Open the file again and write_data i.e. imitate another application
    // restart
    {
        err_code_t err;
        auto lt = create_file_sliding_target(fpath.c_str(), max_file_size,
                                             size_tolerance, level::error, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        write_data(lt, msg0.data(), block_size);
        // Here it should get slided and block_size should get removed
        write_data(lt, msg1.data(), block_size - num0 - num1);
    }
    {
        auto content = read_all(fpath);
        std::string expected(msg1.data(), block_size + num0 + num1);
        expected.append(msg0.data(), block_size);
        expected.append(msg1.data(), block_size - num0 - num1);
        BOOST_REQUIRE_EQUAL(content.size(), expected.size());
        BOOST_CHECK_EQUAL(content, expected);
    }
}

BOOST_AUTO_TEST_CASE(syslog_target)
{
    if (geteuid() == 0)
    {
        // This log_target can't be reliably unit tested.
        // You need to check the dmesg after the tests :).
        // We can read from the dmesg back and try to find if our messages are
        // there but it's too much work for this simple functionality ...
        // You also need to run the tests as superuser in order to be able to
        // write_data to the dmesg.
        constexpr char short_msg[] = "Hey dmesg, this is a short message\n";
        constexpr char long_msg[] =
            "Hey dmesg, this is a "
            "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
            "oooo"
            "ooooooooooooooooooooooooooooooooooooooooong message\n";
        static_assert(sizeof(long_msg) > 512, "");

        err_code_t err;
        auto lt = create_syslog_target(level::fatal, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
        BOOST_CHECK_EQUAL(lt.impl()->max_log_level(), to_number(level::fatal));

        write_data(lt, short_msg, sizeof(short_msg));
        write_data(lt, long_msg, sizeof(long_msg));
    }
    else
    {
        std::cerr << "The test 'syslog_target' requires root privileges\n";
    }
}

BOOST_AUTO_TEST_SUITE_END()
