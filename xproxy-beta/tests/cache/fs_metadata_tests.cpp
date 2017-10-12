#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/aligned_data_ptr.h"
#include "../../cache/disk_reader.h"
#include "../../cache/fs_metadata.h"
#include "../../cache/memory_writer.h"
#include "../../cache/volume_fd.h"
#include "../../cache/volume_info.h"

using namespace cache::detail;

namespace
{

fs_node_key_t gen_key(const char* s) noexcept
{
    return fs_node_key_t{s, strlen(s)};
}

constexpr volume_blocks64_t operator""_vblocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_blocks(v);
}

struct fixture
{
    static constexpr auto path                  = "/tmp/fs_metadata_tests";
    static constexpr bytes64_t min_avg_obj_size = min_obj_size;
    static constexpr bytes64_t volume_size      = min_volume_size;

    std::unique_ptr<fs_metadata> fs_meta_;
    volume_fd fd_;
    volume_info vi_{path};

    fixture() noexcept
    {
        err_code_t err;
        { // Touch file
            std::ofstream f(path);
            BOOST_REQUIRE(!f.fail());
        }
        BOOST_REQUIRE_MESSAGE(fd_.open(path, err), err.message());
        BOOST_REQUIRE_MESSAGE(fd_.truncate(volume_size, err), err.message());
        try
        {
            vi_      = load_check_volume_info(path);
            fs_meta_ = std::make_unique<fs_metadata>(vi_, min_avg_obj_size);
            fs_meta_->clean_init(volume_skip_bytes);
        }
        catch (const std::exception& ex)
        {
            BOOST_REQUIRE_MESSAGE(false, ex.what());
        }
    }
};

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(fs_metadata_tests, fixture)

BOOST_AUTO_TEST_CASE(copy_construct)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_write_pos(15 * store_block_size);

    fs_metadata fs_meta2(*fs_meta_);

    range_vector rv, rv2;
    fs_meta_->read_table_entries(key1, [&](const range_vector& v) mutable
                                 {
                                     rv = v;
                                 });
    fs_meta2.read_table_entries(key1, [&](const range_vector& v) mutable
                                {
                                    rv2 = v;
                                });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));

    fs_meta_->read_table_entries(key2, [&](const range_vector& v) mutable
                                 {
                                     rv = v;
                                 });
    fs_meta2.read_table_entries(key2, [&](const range_vector& v) mutable
                                {
                                    rv2 = v;
                                });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));
    BOOST_CHECK_EQUAL(fs_meta2.sync_serial(), fs_meta_->sync_serial());
    BOOST_CHECK_EQUAL(fs_meta2.write_pos(), fs_meta_->write_pos());
    BOOST_CHECK_EQUAL(fs_meta2.is_dirty(), fs_meta_->is_dirty());
}

BOOST_AUTO_TEST_CASE(save_load_success_var_A)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_write_pos(15 * store_block_size);
    { // Save A metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        err_code_t err;
        fd_.write(mem.get(), mw.written(), 0, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }

    fs_meta_->inc_sync_serial();
    fs_meta_->inc_write_pos(1 * store_block_size);
    { // Save B metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        err_code_t err;
        fd_.write(mem.get(), mw.written(), fs_meta_->max_size_on_disk(), err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }
    err_code_t skip;
    fd_.close(skip);

    disk_reader rdr(path, 0, 2 * fs_meta_->max_size_on_disk());

    fs_metadata fs_meta2(vi_, min_avg_obj_size);
    fs_meta2.clean_init(volume_skip_bytes);
    try
    {
        const bool r = fs_meta2.load(rdr);
        BOOST_REQUIRE(r);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }

    // Now check if the two metadatas has the same entries
    range_vector rv, rv2;
    fs_meta_->read_table_entries(key1, [&](const range_vector& v) mutable
                                 {
                                     rv = v;
                                 });
    fs_meta2.read_table_entries(key1, [&](const range_vector& v) mutable
                                {
                                    rv2 = v;
                                });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));

    fs_meta_->read_table_entries(key2, [&](const range_vector& v) mutable
                                 {
                                     rv = v;
                                 });
    fs_meta2.read_table_entries(key2, [&](const range_vector& v) mutable
                                {
                                    rv2 = v;
                                });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));
    // Check that the B copy of the metadata has been read
    BOOST_CHECK_EQUAL(fs_meta2.sync_serial(), 4);
    BOOST_CHECK_EQUAL(fs_meta2.write_pos(),
                      volume_skip_bytes + 16 * store_block_size);
    BOOST_CHECK_EQUAL(fs_meta2.is_dirty(), false);
}

BOOST_AUTO_TEST_CASE(save_load_success_var_B)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_write_pos(15 * store_block_size);
    { // Save A metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        err_code_t err;
        fd_.write(mem.get(), mw.written(), 0, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }

    fs_meta_->dec_sync_serial(); // Will cause the A copy to be used.
    fs_meta_->inc_write_pos(1 * store_block_size);
    { // Save B metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        err_code_t err;
        fd_.write(mem.get(), mw.written(), fs_meta_->max_size_on_disk(), err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }
    err_code_t skip;
    fd_.close(skip);

    disk_reader rdr(path, 0, 2 * fs_meta_->max_size_on_disk());

    fs_metadata fs_meta2(vi_, min_avg_obj_size);
    fs_meta2.clean_init(volume_skip_bytes);
    try
    {
        const bool r = fs_meta2.load(rdr);
        BOOST_REQUIRE(r);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }

    // Now check if the two metadatas has the same entries
    range_vector rv, rv2;
    fs_meta_->read_table_entries(key1, [&](const range_vector& v) mutable
                                 {
                                     rv = v;
                                 });
    fs_meta2.read_table_entries(key1, [&](const range_vector& v) mutable
                                {
                                    rv2 = v;
                                });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));

    fs_meta_->read_table_entries(key2, [&](const range_vector& v) mutable
                                 {
                                     rv = v;
                                 });
    fs_meta2.read_table_entries(key2, [&](const range_vector& v) mutable
                                {
                                    rv2 = v;
                                });
    BOOST_REQUIRE(!rv.empty());
    BOOST_REQUIRE_EQUAL(rv.size(), rv2.size());
    BOOST_CHECK(std::equal(rv.begin(), rv.end(), rv2.begin()));
    // Check that the A copy of the metadata has been read
    BOOST_CHECK_EQUAL(fs_meta2.sync_serial(), 3);
    BOOST_CHECK_EQUAL(fs_meta2.write_pos(),
                      volume_skip_bytes + 15 * store_block_size);
}

BOOST_AUTO_TEST_CASE(save_load_fail_wrong_hdr)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_write_pos(15 * store_block_size);
    { // Save A metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        // Corrupt the header of A copy.
        // This will provoke failure to load the metadata later.
        ::memset(mem.get(), 0xFF, sizeof(fs_metadata_hdr));

        err_code_t err;
        fd_.write(mem.get(), mw.written(), 0, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }

    fs_meta_->dec_sync_serial(); // Will cause the A copy to be used.
    fs_meta_->inc_write_pos(1 * store_block_size);
    { // Save B metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        err_code_t err;
        fd_.write(mem.get(), mw.written(), fs_meta_->max_size_on_disk(), err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }
    err_code_t skip;
    fd_.close(skip);

    disk_reader rdr(path, 0, 2 * fs_meta_->max_size_on_disk());

    fs_metadata fs_meta2(vi_, min_avg_obj_size);
    fs_meta2.clean_init(volume_skip_bytes);
    try
    {
        const bool r = fs_meta2.load(rdr);
        BOOST_REQUIRE(!r);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }
}

BOOST_AUTO_TEST_CASE(save_load_fail_wrong_ftr)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1  = gen_key("aaa");
    const auto key2  = gen_key("bbb");
    const auto rng11 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng12 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng13 = make_range_elem(40_KB, 20_KB, 96_vblocks);
    const auto rng21 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng22 = make_range_elem(20_KB, 20_KB, 64_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng11, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng12, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng13, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng21, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key2, rng22, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_sync_serial();
    fs_meta_->inc_write_pos(15 * store_block_size);
    { // Save A metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        err_code_t err;
        fd_.write(mem.get(), mw.written(), 0, err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }

    fs_meta_->dec_sync_serial(); // Will cause the A copy to be used.
    fs_meta_->inc_write_pos(1 * store_block_size);
    { // Save B metadata
        auto mem = alloc_page_aligned(fs_meta_->max_size_on_disk());
        memory_writer mw(mem.get(), fs_meta_->max_size_on_disk());
        fs_meta_->save(mw);

        // Corrupt the footer of B copy.
        // This will provoke failure to load the metadata later.
        ::memset(mem.get() + mw.written() - store_block_size, 0xFF,
                 sizeof(fs_metadata_ftr));

        err_code_t err;
        fd_.write(mem.get(), mw.written(), fs_meta_->max_size_on_disk(), err);
        BOOST_REQUIRE_MESSAGE(!err, err.message());
    }
    err_code_t skip;
    fd_.close(skip);

    disk_reader rdr(path, 0, 2 * fs_meta_->max_size_on_disk());

    fs_metadata fs_meta2(vi_, min_avg_obj_size);
    fs_meta2.clean_init(volume_skip_bytes);
    try
    {
        const bool r = fs_meta2.load(rdr);
        BOOST_REQUIRE(!r);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }
}

// The save-load failure of corrupted fs_table is already tested

BOOST_AUTO_TEST_SUITE_END()
