#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../../cache/cache_fs_operations.h"
#include "../../cache/agg_meta_entry.h"
#include "../../cache/agg_writer.h"
#include "../../cache/aligned_data_ptr.h"
#include "../../cache/fs_metadata.h"
#include "../../cache/object_frag_hdr.h"
#include "../../cache/object_key.h"
#include "../../cache/read_transaction.h"
#include "../../cache/write_transaction.h"
#include "../../cache/volume_fd.h"
#include "../../cache/volume_info.h"

using namespace cache::detail;

namespace
{

fs_node_key_t gen_key(const char* s) noexcept
{
    return fs_node_key_t{s, strlen(s)};
}

object_key make_object_key(const char* s, bytes64_t off, bytes64_t len) noexcept
{
    return object_key{gen_key(s), range{off, len}};
}

constexpr volume_blocks64_t operator""_vblocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_blocks(v);
}

constexpr volume_blocks64_t bytes2blocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_bytes(v);
}

struct fixture
{
    static constexpr bytes64_t min_avg_obj_size = min_obj_size;
    static constexpr bytes64_t volume_size      = min_volume_size;
    static constexpr auto data_offset = bytes2blocks(1_MB);
    static constexpr auto cnt_data_blocks =
        bytes2blocks(min_volume_size - 1_MB);

    const boost::container::string path_ = "/tmp/cache_fs_operations_tests";
    fs_metadata_sync_t fs_meta_;
    agg_writer agg_wr_;
    cache_fs_operations fs_ops_;

    fs_metadata make_fs_metadata() noexcept
    {
        err_code_t err;
        volume_fd fd;
        BOOST_REQUIRE_MESSAGE(fd.open(path_.c_str(), err), err.message());
        BOOST_REQUIRE_MESSAGE(fd.truncate(volume_size, err), err.message());
        try
        {
            auto vi = load_check_volume_info(path_);
            fs_metadata fsm(vi, min_avg_obj_size);
            fsm.clean_init(data_offset.to_bytes());
            return fsm;
        }
        catch (const std::exception& ex)
        {
            BOOST_REQUIRE_MESSAGE(false, ex.what());
            abort();
        }
    }

    auto write_pos() noexcept { return bytes2blocks(fs_meta_->write_pos()); }
    auto write_lap() noexcept { return fs_meta_->write_lap(); }

    fixture() noexcept
        : fs_meta_(make_fs_metadata()),
          agg_wr_(write_pos(), write_lap()),
          fs_ops_(
              nullptr, &fs_meta_, nullptr, &path_, data_offset, cnt_data_blocks)
    {
        fs_ops_.set_agg_writer(&agg_wr_);
    }
};
constexpr volume_blocks64_t fixture::data_offset;
constexpr volume_blocks64_t fixture::cnt_data_blocks;

} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(cache_fs_operations_tests, fixture)

BOOST_AUTO_TEST_CASE(lock_vmtx_when_in_agg_write_area)
{
    constexpr auto aggw_area_size = 3 * agg_write_block_size;

    // The write position is at the beginning and we'll be inside
    auto curr_offs = data_offset.to_bytes();
    bool locked = fs_ops_.vmtx_lock_shared(curr_offs);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + aggw_area_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + aggw_area_size);
    BOOST_REQUIRE(!locked);

    // Now move the write position somewhere inside the volume.
    curr_offs = cnt_data_blocks.to_bytes() / 2;
    auto wpos = fs_meta_->write_pos();
    fs_meta_->inc_write_pos(curr_offs - wpos);
    locked = fs_ops_.vmtx_lock_shared(curr_offs);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + aggw_area_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + aggw_area_size);
    BOOST_REQUIRE(!locked);

    // Now move the write position one aggregate area before the end
    curr_offs = fs_ops_.end_data_offs() - aggw_area_size;
    wpos = fs_meta_->write_pos();
    fs_meta_->inc_write_pos(curr_offs - wpos);
    locked = fs_ops_.vmtx_lock_shared(curr_offs);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + aggw_area_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    // Going at the beginning won't be included
    locked = fs_ops_.vmtx_lock_shared(fs_ops_.data_offs());
    BOOST_REQUIRE(!locked);

    // Move one aggregate window forward
    curr_offs = fs_ops_.end_data_offs() - 2 * agg_write_block_size;
    wpos = fs_meta_->write_pos();
    fs_meta_->inc_write_pos(agg_write_block_size);
    locked = fs_ops_.vmtx_lock_shared(curr_offs);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + 2 * agg_write_block_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    // Going at the beginning will be included
    locked = fs_ops_.vmtx_lock_shared(fs_ops_.data_offs());
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(fs_ops_.data_offs() +
                                      agg_write_block_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked =
        fs_ops_.vmtx_lock_shared(fs_ops_.data_offs() + agg_write_block_size);
    BOOST_REQUIRE(!locked);

    // Move at the last aggregation window
    curr_offs = fs_ops_.end_data_offs() - agg_write_block_size;
    wpos = fs_meta_->write_pos();
    fs_meta_->inc_write_pos(agg_write_block_size);
    locked = fs_ops_.vmtx_lock_shared(curr_offs);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(curr_offs + agg_write_block_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    // Going at the beginning will be included
    locked = fs_ops_.vmtx_lock_shared(fs_ops_.data_offs());
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(fs_ops_.data_offs() +
                                      2 * agg_write_block_size - 1);
    BOOST_REQUIRE(locked);
    fs_ops_.vmtx_unlock_shared();
    locked = fs_ops_.vmtx_lock_shared(fs_ops_.data_offs() +
                                      2 * agg_write_block_size);
    BOOST_REQUIRE(!locked);
}

BOOST_AUTO_TEST_CASE(begin_end_read_success)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng2 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng3 = make_range_elem(40_KB, 20_KB, 96_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Now begin-end
    auto rtrans = fs_ops_.fsmd_begin_read(make_object_key("aaa", 10_KB, 32_KB));
    BOOST_CHECK(rtrans.valid());

    bool found = false;
    fs_meta_->read_table_entries(key1, [&](const auto& rvec)
                                 {
                                     found = true;
                                     for (const auto& re : rvec)
                                         BOOST_CHECK_EQUAL(re.cnt_readers(), 1);
                                 });
    BOOST_CHECK(found);

    fs_ops_.fsmd_end_read(std::move(rtrans));
    BOOST_CHECK(!rtrans.valid());
}

BOOST_AUTO_TEST_CASE(begin_read_fail_no_data)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng2 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng3 = make_range_elem(50_KB, 20_KB, 96_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Now begin-end
    // Don't found not present key
    auto rtrans = fs_ops_.fsmd_begin_read(make_object_key("aab", 10_KB, 32_KB));
    BOOST_CHECK(!rtrans.valid());
    // Don't found non present range
    rtrans = fs_ops_.fsmd_begin_read(make_object_key("aaa", 100_KB, 32_KB));
    BOOST_CHECK(!rtrans.valid());
    // Don't found range which has a hole
    rtrans = fs_ops_.fsmd_begin_read(make_object_key("aaa", 30_KB, 32_KB));
    BOOST_CHECK(!rtrans.valid());
}

BOOST_AUTO_TEST_CASE(begin_read_fail_max_cnt_readers)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, 20_KB, 32_vblocks);
    const auto rng2 = make_range_elem(20_KB, 20_KB, 64_vblocks);
    const auto rng3 = make_range_elem(40_KB, 20_KB, 96_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Make one of the entries read to the max possible readers
    bool found = false;
    fs_meta_->modify_table_entries(key1, [&](const auto& rvec)
                                   {
                                       found = true;
                                       rv_elem_atomic_inc_readers(rvec.begin());
                                       auto re = rvec.begin() + 1;
                                       for (; rv_elem_atomic_inc_readers(re);)
                                       {
                                       }
                                   });
    BOOST_REQUIRE(found);

    // Now begin will fail because of the readers max has been reached
    auto rtrans = fs_ops_.fsmd_begin_read(make_object_key("aaa", 10_KB, 32_KB));
    BOOST_CHECK(!rtrans.valid());

    found = false;
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found = true;
            BOOST_CHECK_EQUAL(std::next(rvec.begin(), 0)->cnt_readers(), 1);
            BOOST_CHECK_EQUAL(std::next(rvec.begin(), 1)->cnt_readers(), 255);
            BOOST_CHECK_EQUAL(std::next(rvec.begin(), 2)->cnt_readers(), 0);
        });
    BOOST_CHECK(found);
}

BOOST_AUTO_TEST_CASE(find_next_and_next_range_elem)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, 20_KB, 1024_vblocks);
    const auto rng2 = make_range_elem(20_KB, 20_KB, 2048_vblocks);
    const auto rng3 = make_range_elem(40_KB, 20_KB, 4096_vblocks);

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Now begin-end
    auto rtrans = fs_ops_.fsmd_begin_read(make_object_key("aaa", 10_KB, 32_KB));
    BOOST_CHECK(rtrans.valid());

    auto ret = fs_ops_.fsmd_find_next_range_elem(rtrans);
    BOOST_REQUIRE(ret);
    BOOST_CHECK_EQUAL(ret.value(), rng1);
    rtrans.inc_read_bytes(10_KB);
    ret = fs_ops_.fsmd_find_next_range_elem(rtrans);
    BOOST_REQUIRE(ret);
    BOOST_CHECK_EQUAL(ret.value(), rng2);
    rtrans.inc_read_bytes(20_KB);
    ret = fs_ops_.fsmd_find_next_range_elem(rtrans);
    BOOST_REQUIRE(ret);
    BOOST_CHECK_EQUAL(ret.value(), rng3);
    rtrans.inc_read_bytes(2_KB);
    BOOST_REQUIRE(rtrans.finished());

    fs_ops_.fsmd_end_read(std::move(rtrans));
    BOOST_CHECK(!rtrans.valid());
}

BOOST_AUTO_TEST_CASE(rem_non_evac_fragments_filtered_from_fs_meta)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    auto doff       = data_offset;
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng2 = make_range_elem(20_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng3 = make_range_elem(40_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng4 = make_range_elem(60_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng5 = make_range_elem(80_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng4, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng5, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    auto rtrans1 =
        fs_ops_.fsmd_begin_read(make_object_key("aaa", 10_KB, 22_KB));
    BOOST_CHECK(rtrans1.valid());
    auto rtrans2 =
        fs_ops_.fsmd_begin_read(make_object_key("aaa", 60_KB, 16_KB));
    BOOST_CHECK(rtrans2.valid());

    std::vector<agg_meta_entry> evac_frags;
    // This is going to be kept because there is a reader for it
    evac_frags.emplace_back(key1, rng1);
    // This is going to be filtered out, because it won't be found by key
    evac_frags.emplace_back(gen_key("bbb"), rng1);
    // This is going to be kept because there is a reader for it
    evac_frags.emplace_back(key1, rng2);
    // This is going to be filtered out because it's present but there is
    // no readers for it.
    evac_frags.emplace_back(key1, rng3);
    // This is going to be kept because there is a reader for it
    evac_frags.emplace_back(key1, rng4);
    // This is going to be filtered out because it's present but there is
    // no readers for it.
    evac_frags.emplace_back(key1, rng5);
    // This is going to be filtered out because the range is not present
    evac_frags.emplace_back(key1, make_range_elem(200_KB, 20_KB, doff));

    fs_ops_.fsmd_rem_non_evac_frags(evac_frags, data_offset,
                                    doff - data_offset);
    BOOST_REQUIRE_EQUAL(evac_frags.size(), 3);
    BOOST_CHECK_EQUAL(evac_frags[0].key(), key1);
    BOOST_CHECK_EQUAL(evac_frags[0].rng(), rng1);
    BOOST_CHECK_EQUAL(evac_frags[1].key(), key1);
    BOOST_CHECK_EQUAL(evac_frags[1].rng(), rng2);
    BOOST_CHECK_EQUAL(evac_frags[2].key(), key1);
    BOOST_CHECK_EQUAL(evac_frags[2].rng(), rng4);

    fs_ops_.fsmd_end_read(std::move(rtrans1));
    BOOST_CHECK(!rtrans1.valid());
    fs_ops_.fsmd_end_read(std::move(rtrans2));
    BOOST_CHECK(!rtrans2.valid());
}

BOOST_AUTO_TEST_CASE(rem_non_evac_fragments_filtered_invalid_ranges)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    auto doff       = data_offset;
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng2 = make_range_elem(20_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng3 = make_range_elem(40_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng4 = make_range_elem(60_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);
    const auto rng5 = make_range_elem(80_KB, 20_KB, doff);
    doff += bytes2blocks(20_KB);

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng4, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng5, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    auto rtrans1 =
        fs_ops_.fsmd_begin_read(make_object_key("aaa", 10_KB, 22_KB));
    BOOST_CHECK(rtrans1.valid());
    auto rtrans2 =
        fs_ops_.fsmd_begin_read(make_object_key("aaa", 60_KB, 16_KB));
    BOOST_CHECK(rtrans2.valid());

    std::vector<agg_meta_entry> evac_frags;
    // This is going to be kept because there is a reader for it
    evac_frags.emplace_back(key1, rng1);
    // This is going to be filtered out, because it won't be found by key
    evac_frags.emplace_back(gen_key("bbb"), rng1);
    // This is going to be kept because there is a reader for it
    evac_frags.emplace_back(key1, rng2);
    // This is going to be filtered out because it's present but there is
    // no readers for it.
    evac_frags.emplace_back(key1, rng3);
    // This is going to be filtered because lays outside the checked disk area
    evac_frags.emplace_back(key1, rng4);
    // This is going to be filtered out because it's present but there is
    // no readers for it.
    evac_frags.emplace_back(key1, rng5);
    // This is going to be filtered out because the range is not present
    evac_frags.emplace_back(key1, make_range_elem(200_KB, 20_KB, doff));

    fs_ops_.fsmd_rem_non_evac_frags(evac_frags, data_offset,
                                    rng4.disk_offset() - data_offset);
    BOOST_REQUIRE_EQUAL(evac_frags.size(), 2);
    BOOST_CHECK_EQUAL(evac_frags[0].key(), key1);
    BOOST_CHECK_EQUAL(evac_frags[0].rng(), rng1);
    BOOST_CHECK_EQUAL(evac_frags[1].key(), key1);
    BOOST_CHECK_EQUAL(evac_frags[1].rng(), rng2);

    fs_ops_.fsmd_end_read(std::move(rtrans1));
    BOOST_CHECK(!rtrans1.valid());
    fs_ops_.fsmd_end_read(std::move(rtrans2));
    BOOST_CHECK(!rtrans2.valid());
}

BOOST_AUTO_TEST_CASE(add_evac_fragment_success)
{
    // First fill something into the metadata
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };

    const auto rlen = bytes2blocks(20_KB);
    auto doff       = data_offset;
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng3 = make_range_elem(60_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng4 = make_range_elem(90_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng5 = make_range_elem(120_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    auto res = fs_meta_->add_table_entry(key1, rng1, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng2, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng3, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng4, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng5, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Move the write position to a new location so that the added fragments
    // receive new disk location.
    const auto inc      = bytes2blocks(6_MB);
    const auto new_offs = data_offset + inc;
    fs_meta_->inc_write_pos(inc.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_evac_fragment(key1, to_range(rng1), fbuf,
                                            new_offs, wblock);
    BOOST_REQUIRE(r);
    r = fs_ops_.fsmd_add_evac_fragment(key1, to_range(rng3), fbuf, new_offs,
                                       wblock);
    BOOST_REQUIRE(r);
    r = fs_ops_.fsmd_add_evac_fragment(key1, to_range(rng5), fbuf, new_offs,
                                       wblock);
    BOOST_REQUIRE(r);

    // Lastly, check that the evacuated entries has received new disk offsets
    bool found = false;
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found         = true;
            auto exp_offs = new_offs + bytes2blocks(agg_write_meta_size);
            BOOST_REQUIRE_EQUAL(rvec.size(), 5);
            BOOST_REQUIRE(std::next(rvec.begin(), 0)->in_memory());
            BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), 0)->disk_offset(),
                                exp_offs);
            exp_offs += bytes2blocks(object_frag_size(rlen.to_bytes()));
            // This range hasn't been evacutated
            BOOST_REQUIRE(!std::next(rvec.begin(), 1)->in_memory());
            BOOST_REQUIRE_EQUAL(*std::next(rvec.begin(), 1), rng2);
            BOOST_REQUIRE(std::next(rvec.begin(), 2)->in_memory());
            BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), 2)->disk_offset(),
                                exp_offs);
            exp_offs += bytes2blocks(object_frag_size(rlen.to_bytes()));
            // This range hasn't been evacutated
            BOOST_REQUIRE(!std::next(rvec.begin(), 3)->in_memory());
            BOOST_REQUIRE_EQUAL(*std::next(rvec.begin(), 3), rng4);
            BOOST_REQUIRE(std::next(rvec.begin(), 4)->in_memory());
            BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), 4)->disk_offset(),
                                exp_offs);
        });
    BOOST_REQUIRE(found);
}

BOOST_AUTO_TEST_CASE(add_new_fragment_success)
{
    // First fill something into the metadata
    // Move the write position to a new location so that the added fragments
    // receive new disk location.
    const auto inc       = bytes2blocks(6_MB);
    const auto disk_offs = data_offset + inc;
    auto doff = data_offset + inc;
    fs_meta_->inc_write_pos(inc.to_bytes());

    const auto rlen = bytes2blocks(20_KB);
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng3 = make_range_elem(60_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng4 = make_range_elem(90_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng5 = make_range_elem(120_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_new_fragment(key1, to_range(rng1), fbuf,
                                           disk_offs, wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng2), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng3), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng4), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng5), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);

    // Lastly, check that the evacuated entries has received new disk offsets
    bool found = false;
    // Check also the entries in the agg_write_block.
    auto entries = wblock->end_disk_write(); // It's incorrect, but we need them
    BOOST_REQUIRE_EQUAL(entries.size(), 5);
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found         = true;
            auto exp_offs = disk_offs + bytes2blocks(agg_write_meta_size);
            BOOST_REQUIRE_EQUAL(rvec.size(), 5);
            for (auto i = 0U; i < rvec.size(); ++i)
            {
                BOOST_REQUIRE_EQUAL(entries[i].key(), key1);
                BOOST_REQUIRE_EQUAL(entries[i].rng(),
                                    *std::next(rvec.begin(), i));
                BOOST_REQUIRE(std::next(rvec.begin(), i)->in_memory());
                BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), i)->disk_offset(),
                                    exp_offs);
                exp_offs += bytes2blocks(object_frag_size(rlen.to_bytes()));
            }
        });
    BOOST_REQUIRE(found);
}

BOOST_AUTO_TEST_CASE(add_new_fragment_overwrite_meta)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };
    // First fill something into the metadata
    // Move the write position to a new location so that the added fragments
    // receive new disk location.
    const auto inc       = bytes2blocks(6_MB);
    const auto disk_offs = data_offset + inc;
    auto doff = data_offset + inc;
    fs_meta_->inc_write_pos(inc.to_bytes());

    const auto rlen = bytes2blocks(20_KB);
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng3 = make_range_elem(60_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng4 = make_range_elem(32_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng5 = make_range_elem(64_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    // Add two fake fragments in the metadata which is going to be overwritten
    auto res = fs_meta_->add_table_entry(key1, rng4, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng5, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_new_fragment(key1, to_range(rng1), fbuf,
                                           disk_offs, wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng2), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng3), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);

    // Lastly, check that the evacuated entries has received new disk offsets
    bool found = false;
    // Check also the entries in the agg_write_block.
    auto entries = wblock->end_disk_write(); // It's incorrect, but we need them
    BOOST_REQUIRE_EQUAL(entries.size(), 3);
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found         = true;
            auto exp_offs = disk_offs + bytes2blocks(agg_write_meta_size);
            BOOST_REQUIRE_EQUAL(rvec.size(), 3);
            for (auto i = 0U; i < rvec.size(); ++i)
            {
                BOOST_REQUIRE_EQUAL(entries[i].key(), key1);
                BOOST_REQUIRE_EQUAL(entries[i].rng(),
                                    *std::next(rvec.begin(), i));
                BOOST_REQUIRE(std::next(rvec.begin(), i)->in_memory());
                BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), i)->disk_offset(),
                                    exp_offs);
                exp_offs += bytes2blocks(object_frag_size(rlen.to_bytes()));
            }
        });
    BOOST_REQUIRE(found);
}

BOOST_AUTO_TEST_CASE(add_new_fragment_overlaps_in_agg_write_block)
{
    // First fill something into the metadata
    // Move the write position to a new location so that the added fragments
    // receive new disk location.
    const auto inc       = bytes2blocks(6_MB);
    const auto disk_offs = data_offset + inc;
    auto doff = data_offset + inc;
    fs_meta_->inc_write_pos(inc.to_bytes());

    const auto rlen = bytes2blocks(20_KB);
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    // This one will fail due to the overlap
    const auto rng3 = make_range_elem(40_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_new_fragment(key1, to_range(rng1), fbuf,
                                           disk_offs, wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng2), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    // Currently the API hides that the fragment is not added returning true.
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng3), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);

    // Make sure that the fragment hasn't been added
    // Lastly, check that the evacuated entries has received new disk offsets
    bool found = false;
    // Check also the entries in the agg_write_block.
    auto entries = wblock->end_disk_write(); // It's incorrect, but we need them
    BOOST_REQUIRE_EQUAL(entries.size(), 2);
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found          = true;
            auto exp_offs  = disk_offs + bytes2blocks(agg_write_meta_size);
            auto init_offs = 0U;
            BOOST_REQUIRE_EQUAL(rvec.size(), 2);
            for (auto i = 0U; i < rvec.size(); ++i, init_offs += 30_KB)
            {
                BOOST_REQUIRE_EQUAL(entries[i].key(), key1);
                BOOST_REQUIRE_EQUAL(entries[i].rng().rng_offset(), init_offs);
                BOOST_REQUIRE_EQUAL(entries[i].rng(),
                                    *std::next(rvec.begin(), i));
                BOOST_REQUIRE(std::next(rvec.begin(), i)->in_memory());
                BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), i)->disk_offset(),
                                    exp_offs);
                exp_offs += bytes2blocks(object_frag_size(rlen.to_bytes()));
            }
        });
    BOOST_REQUIRE(found);
}

BOOST_AUTO_TEST_CASE(add_new_fragment_skip_wont_overwrite_meta)
{
    auto overwrite_dont_call = [](const auto&, const auto&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called");
        return true;
    };
    // First fill something into the metadata
    // Move the write position to a new location so that the added fragments
    // receive new disk location.
    const auto inc       = bytes2blocks(6_MB);
    const auto disk_offs = data_offset + inc;
    auto doff = data_offset + inc;
    fs_meta_->inc_write_pos(inc.to_bytes());

    const auto rlen = bytes2blocks(20_KB);
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng3 = make_range_elem(60_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng4 = make_range_elem(32_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng5 = make_range_elem(64_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    // Add two fake fragments in the metadata which is going to be overwritten
    auto res = fs_meta_->add_table_entry(key1, rng4, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);
    res = fs_meta_->add_table_entry(key1, rng5, overwrite_dont_call);
    BOOST_CHECK(res == fs_table::add_res::added);

    // Mark the fake fragments as read
    auto rtrans1 =
        fs_ops_.fsmd_begin_read(make_object_key("aaa", 32_KB, rlen.to_bytes()));
    BOOST_CHECK(rtrans1.valid());
    auto rtrans2 =
        fs_ops_.fsmd_begin_read(make_object_key("aaa", 64_KB, rlen.to_bytes()));
    BOOST_CHECK(rtrans2.valid());

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_new_fragment(key1, to_range(rng1), fbuf,
                                           disk_offs, wblock);
    BOOST_REQUIRE(r);
    // These two fragments will fail to be added, but the API will hide this
    // returning true.
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng2), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng3), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);

    // Lastly, check that the evacuated entries has received new disk offsets
    bool found = false;
    // Check also the entries in the agg_write_block.
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found = true;
            BOOST_REQUIRE_EQUAL(rvec.size(), 3);
            // Only the first entry has been added to the in-memory metadata.
            // The other twos are the fake ones.
            BOOST_REQUIRE(std::next(rvec.begin(), 0)->in_memory());
            BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), 0)->rng_offset(),
                                rng1.rng_offset());
            BOOST_REQUIRE(!std::next(rvec.begin(), 1)->in_memory());
            BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), 1)->rng_offset(),
                                rng4.rng_offset());
            BOOST_REQUIRE(!std::next(rvec.begin(), 2)->in_memory());
            BOOST_REQUIRE_EQUAL(std::next(rvec.begin(), 2)->rng_offset(),
                                rng5.rng_offset());
        });
    BOOST_REQUIRE(found);
}

BOOST_AUTO_TEST_CASE(commit_disk_writes_no_wrap_wpos)
{
    const auto inc       = bytes2blocks(6_MB);
    const auto disk_offs = data_offset + inc;
    auto doff = data_offset + inc;
    fs_meta_->inc_write_pos(inc.to_bytes());

    const auto rlen = bytes2blocks(20_KB);
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng3 = make_range_elem(60_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng4 = make_range_elem(90_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng5 = make_range_elem(120_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    // Make two write transactions corresponding to the above 5 ranges.
    // They are not checked currently, but let's add them for completeness.
    std::vector<write_transaction> wtranss;
    wtranss.push_back(
        fs_ops_.fsmd_begin_write(make_object_key("aaa", 0_KB, 89_KB)));
    BOOST_REQUIRE(wtranss.back().valid());
    wtranss.push_back(
        fs_ops_.fsmd_begin_write(make_object_key("aaa", 90_KB, 60_KB)));
    BOOST_REQUIRE(wtranss.back().valid());

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_new_fragment(key1, to_range(rng1), fbuf,
                                           disk_offs, wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng2), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng3), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng4), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng5), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);

    // Remove the middle fragment to simulate the situation when one of the
    // finished fragments is not present in the in-memory metadata.
    fs_meta_->rem_table_entries(key1, [](auto& rvec)
                                {
                                    BOOST_REQUIRE_EQUAL(rvec.size(), 5);
                                    rvec.rem_range(rvec.begin() + 2);
                                });

    // All entries must be in the memory
    bool found = false;
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found = true;
            BOOST_REQUIRE_EQUAL(rvec.size(), 4);
            for (auto i = 0U; i < rvec.size(); ++i)
                BOOST_REQUIRE(std::next(rvec.begin(), i)->in_memory());
        });
    BOOST_REQUIRE(found);

    const auto prev_lap  = fs_meta_->write_lap();
    const auto prev_wpos = fs_meta_->write_pos();

    auto wpos = fs_ops_.fsmd_commit_disk_write(disk_offs, wtranss,
                                               agg_wr_.write_block());

    // Now all entries must be on the disk
    found = false;
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found = true;
            BOOST_REQUIRE_EQUAL(rvec.size(), 4);
            for (auto i = 0U; i < rvec.size(); ++i)
                BOOST_REQUIRE(!std::next(rvec.begin(), i)->in_memory());
        });
    BOOST_REQUIRE(found);

    // The write pos must have been increased but the write lap must be the same
    BOOST_REQUIRE_EQUAL(wpos.write_pos_, prev_wpos + agg_write_block_size);
    BOOST_REQUIRE_EQUAL(wpos.write_lap_, prev_lap);
    BOOST_REQUIRE_EQUAL(wpos.write_pos_, fs_meta_->write_pos());
    BOOST_REQUIRE_EQUAL(wpos.write_lap_, fs_meta_->write_lap());
}

BOOST_AUTO_TEST_CASE(commit_disk_writes_wrap_wpos)
{
    const auto disk_offs =
        bytes2blocks(fs_ops_.end_data_offs() - agg_write_block_size);
    auto doff = disk_offs;
    fs_meta_->inc_write_pos(doff.to_bytes() - fs_ops_.data_offs());

    const auto rlen = bytes2blocks(20_KB);
    const auto key1 = gen_key("aaa");
    const auto rng1 = make_range_elem(0, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng2 = make_range_elem(30_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng3 = make_range_elem(60_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng4 = make_range_elem(90_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;
    const auto rng5 = make_range_elem(120_KB, rlen.to_bytes(), doff);
    doff += 1024_vblocks;

    // Make two write transactions corresponding to the above 5 ranges.
    // They are not checked currently, but let's add them for completeness.
    std::vector<write_transaction> wtranss;
    wtranss.push_back(
        fs_ops_.fsmd_begin_write(make_object_key("aaa", 0_KB, 89_KB)));
    BOOST_REQUIRE(wtranss.back().valid());
    wtranss.push_back(
        fs_ops_.fsmd_begin_write(make_object_key("aaa", 90_KB, 60_KB)));
    BOOST_REQUIRE(wtranss.back().valid());

    auto frag = alloc_page_aligned(rlen.to_bytes());
    auto fbuf = cache_fs_ops::frag_data_t(frag.get(), rlen.to_bytes());

    // Add the fragments
    auto& wblock = agg_wr_.write_block();
    bool r = fs_ops_.fsmd_add_new_fragment(key1, to_range(rng1), fbuf,
                                           disk_offs, wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng2), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng3), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng4), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);
    fs_ops_.fsmd_add_new_fragment(key1, to_range(rng5), fbuf, disk_offs,
                                  wblock);
    BOOST_REQUIRE(r);

    // All entries must be in the memory
    bool found = false;
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found = true;
            BOOST_REQUIRE_EQUAL(rvec.size(), 5);
            for (auto i = 0U; i < rvec.size(); ++i)
                BOOST_REQUIRE(std::next(rvec.begin(), i)->in_memory());
        });
    BOOST_REQUIRE(found);

    const auto prev_lap  = fs_meta_->write_lap();
    const auto prev_wpos = fs_meta_->write_pos();

    auto wpos = fs_ops_.fsmd_commit_disk_write(disk_offs, wtranss,
                                               agg_wr_.write_block());

    // Now all entries must be on the disk
    found = false;
    fs_meta_->read_table_entries(
        key1, [&](const auto& rvec)
        {
            found = true;
            BOOST_REQUIRE_EQUAL(rvec.size(), 5);
            for (auto i = 0U; i < rvec.size(); ++i)
                BOOST_REQUIRE(!std::next(rvec.begin(), i)->in_memory());
        });
    BOOST_REQUIRE(found);

    // The write pos must have been increased but the write lap must be the same
    BOOST_REQUIRE_NE(wpos.write_pos_, prev_wpos + agg_write_block_size);
    BOOST_REQUIRE_EQUAL(wpos.write_pos_, fs_ops_.data_offs());
    BOOST_REQUIRE_EQUAL(wpos.write_lap_, prev_lap + 1);
    BOOST_REQUIRE_EQUAL(wpos.write_pos_, fs_meta_->write_pos());
    BOOST_REQUIRE_EQUAL(wpos.write_lap_, fs_meta_->write_lap());
}

BOOST_AUTO_TEST_SUITE_END()
