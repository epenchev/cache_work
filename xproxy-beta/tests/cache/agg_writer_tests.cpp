#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "cache_fs_ops_empty.h"
#include "../../cache/agg_writer.h"
#include "../../cache/cache_fs_operations.h"
#include "../../cache/frag_write_buff.h"
#include "../../cache/fs_metadata.h"
#include "../../cache/object_frag_hdr.h"
#include "../../cache/volume_fd.h"
#include "../../cache/volume_info.h"

using namespace x3me::mem_utils;
using namespace cache::detail;
using namespace std::placeholders;

namespace
{

constexpr volume_blocks64_t bytes2blocks(unsigned long long v) noexcept
{
    return volume_blocks64_t::create_from_bytes(v);
}

fs_node_key_t gen_key(const char* s) noexcept
{
    return fs_node_key_t{s, strlen(s)};
}

frag_write_buff make_wbuff(char c, bytes32_t cap, bytes32_t size) noexcept
{
    frag_write_buff buff(cap);
    auto b = buff.buff();
    ::memset(b.data(), c, size);
    buff.commit(size);
    return buff;
}

////////////////////////////////////////////////////////////////////////////////

// A helper class to test the object_write_handle functionality
class fs_ops_mock final : public cache_fs_ops_empty
{
    io_service_t ios_;
    io_service_t::work iow_{ios_};

public:
    bool vmtx_wait_disk_readers_called_ = false;
    bool read_can_be_called_            = false;
    bool write_can_be_called_           = false;

    bytes64_t curr_disk_offs_ = 0;

    std::vector<uint8_t> data_;

    std::function<void(std::vector<agg_meta_entry>&,
                       volume_blocks64_t,
                       volume_blocks64_t)> fsmd_rem_non_evac_frags_;
    std::function<bool(const fs_node_key_t&,
                       const range&,
                       frag_data_t,
                       volume_blocks64_t,
                       agg_wblock_sync_t&)> fsmd_add_evac_fragment_;
    std::function<bool(const fs_node_key_t&,
                       const range&,
                       frag_data_t,
                       volume_blocks64_t,
                       agg_wblock_sync_t&)> fsmd_add_new_fragment_;
    std::function<wr_pos(volume_blocks64_t,
                         const std::vector<write_transaction>&,
                         agg_wblock_sync_t&)> fsmd_commit_disk_write_;
    std::function<void(volume_blocks64_t,
                       const std::vector<write_transaction>&,
                       agg_wblock_sync_t&)> fsmd_fin_flush_commit_;

public:
    fs_ops_mock() noexcept {}

    void run_one() noexcept { ios_.poll_one(); }

private:
    void vmtx_wait_disk_readers() noexcept final
    {
        vmtx_wait_disk_readers_called_ = true;
    }
    void aios_push_front_write_queue(owner_ptr_t<agg_writer> tt) noexcept final
    {
        ios_.post(
            [ this, t = static_cast<aio_task*>(tt) ]
            {
                err_code_t err; // Won't test cache errors
                auto d = t->on_begin_io_op();
                BOOST_REQUIRE_MESSAGE(d, "The agg_writer doesn't skip IO");
                const auto boffs  = d->offs_;
                const auto eoffs  = d->offs_ + d->size_;
                const auto dboffs = curr_disk_offs_;
                const auto deoffs = curr_disk_offs_ + agg_write_block_size;
                BOOST_REQUIRE_MESSAGE(
                    x3me::math::in_range(boffs, eoffs, dboffs, deoffs),
                    "Invalid requested range");
                BOOST_REQUIRE((boffs % store_block_size) == 0);
                BOOST_REQUIRE((d->size_ % volume_block_size) == 0);
                const auto data_offs = boffs - dboffs;
                if (t->operation() == aio_op::read)
                {
                    BOOST_REQUIRE(read_can_be_called_);
                    BOOST_REQUIRE_GE(data_.size(), data_offs + d->size_);
                    ::memcpy(d->buf_, &data_[data_offs], d->size_);
                }
                else if (t->operation() == aio_op::write)
                {
                    BOOST_REQUIRE(write_can_be_called_);
                    if (data_offs + d->size_ > data_.size())
                        data_.resize(data_offs + d->size_);
                    ::memcpy(&data_[data_offs], d->buf_, d->size_);
                }
                else
                {
                    BOOST_REQUIRE_MESSAGE(false, "Must do read or write only");
                }
                t->on_end_io_op(err);
            });
    }
    void fsmd_rem_non_evac_frags(std::vector<agg_meta_entry>& entries,
                                 volume_blocks64_t offs,
                                 volume_blocks64_t size) noexcept override
    {
        fsmd_rem_non_evac_frags_(entries, offs, size);
    }
    bool fsmd_add_evac_fragment(const fs_node_key_t& key,
                                const range& rng,
                                frag_data_t frag,
                                volume_blocks64_t offs,
                                agg_wblock_sync_t& wblock) noexcept override
    {
        return fsmd_add_evac_fragment_(key, rng, frag, offs, wblock);
    }
    bool fsmd_add_new_fragment(const fs_node_key_t& key,
                               const range& rng,
                               frag_data_t frag,
                               volume_blocks64_t offs,
                               agg_wblock_sync_t& wblock) noexcept override
    {
        return fsmd_add_new_fragment_(key, rng, frag, offs, wblock);
    }
    wr_pos fsmd_commit_disk_write(volume_blocks64_t offs,
                                  const std::vector<write_transaction>& wtranss,
                                  agg_wblock_sync_t& wblock) noexcept override
    {
        return fsmd_commit_disk_write_(offs, wtranss, wblock);
    }
    void fsmd_fin_flush_commit(volume_blocks64_t offs,
                               const std::vector<write_transaction>& wtranss,
                               agg_wblock_sync_t& wblock) noexcept override
    {
        fsmd_fin_flush_commit_(offs, wtranss, wblock);
    }
};

////////////////////////////////////////////////////////////////////////////////

struct fixture
{
    static constexpr bytes64_t min_avg_obj_size = min_obj_size;
    static constexpr bytes64_t volume_size      = min_volume_size;
    static constexpr auto data_offset = bytes2blocks(1_MB);
    static constexpr auto cnt_data_blocks =
        bytes2blocks(min_volume_size - 1_MB);

    const boost::container::string path_ = "/tmp/agg_writer_tests";
    fs_metadata_sync_t fs_meta_;
    cache_fs_operations fs_ops_real_;
    fs_ops_mock fs_ops_mock_;

    std::unique_ptr<agg_writer> agg_wr_;

    fs_node_key_t given_key_;
    range given_rng_;
    array_view<const uint8_t> given_frag_;
    volume_blocks64_t given_offs_;
    bool allow_add_new_frag_ = true;

    frag_write_buff exp_evac_wbuff;

    bool fsmd_add_new_fragment_called_  = false;
    bool fsmd_add_evac_fragment_called_ = false;
    bool fsmd_commit_disk_write_called_ = false;

public:
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
          fs_ops_real_(
              nullptr, &fs_meta_, nullptr, &path_, data_offset, cnt_data_blocks)
    {
        fs_ops_mock_.fsmd_add_new_fragment_ =
            [&](const fs_node_key_t& key, const range& rng,
                array_view<const uint8_t> frag, volume_blocks64_t offs,
                agg_wblock_sync_t& wblock)
        {
            fsmd_add_new_fragment_called_ = true;
            given_key_                    = key;
            given_rng_                    = rng;
            given_frag_                   = frag;
            given_offs_                   = offs;
            // Let the real operations handle the real work
            return allow_add_new_frag_ &&
                   fs_ops_real_.fsmd_add_new_fragment(key, rng, frag, offs,
                                                      wblock);
        };
        fs_ops_mock_.fsmd_add_evac_fragment_ =
            [&](const fs_node_key_t& key, const range& rng,
                array_view<const uint8_t> frag, volume_blocks64_t offs,
                agg_wblock_sync_t& wblock)
        {
            fsmd_add_evac_fragment_called_ = true;
            given_key_                     = key;
            given_rng_ = rng;
            BOOST_REQUIRE_EQUAL(frag.size(), exp_evac_wbuff.size());
            BOOST_REQUIRE(0 == ::memcmp(frag.data(), exp_evac_wbuff.data(),
                                        exp_evac_wbuff.size()));
            given_offs_ = offs;
            // Let the real operations handle the real work
            return fs_ops_real_.fsmd_add_evac_fragment(key, rng, frag, offs,
                                                       wblock);
        };
        fs_ops_mock_.fsmd_commit_disk_write_ =
            [&](volume_blocks64_t offs,
                const std::vector<write_transaction>& wtranss,
                agg_wblock_sync_t& wblock)
        {
            fsmd_commit_disk_write_called_ = true;
            given_offs_                    = offs;
            // Let the real operations handle the real work
            return fs_ops_real_.fsmd_commit_disk_write(offs, wtranss, wblock);
        };
    }

    void reset_agg_wr(volume_blocks64_t wr_off, uint64_t wr_lap)
    {
        agg_wr_ = std::make_unique<agg_writer>(wr_off, wr_lap);
        agg_wr_->start(&fs_ops_mock_);
        fs_meta_->set_write_pos(wr_off.to_bytes(), wr_lap);
    }
};
constexpr volume_blocks64_t fixture::data_offset;
constexpr volume_blocks64_t fixture::cnt_data_blocks;

} // namespace
////////////////////////////////////////////////////////////////////////////////
// Completely agree that the tests here are the ugliest unit tests ever made.
// I won't even call them unit tests, but they are needed to check that the
// aggregate writer state machine works correctly in MOST situations.
// The situations reproduced here are very hard to reproduce otherwise.
// However, the maintenance of these tests will be nightmare.

BOOST_FIXTURE_TEST_SUITE(agg_writer_tests, fixture)

BOOST_AUTO_TEST_CASE(first_lap_dont_read_to_evacuate_only_write)
{
    // First setup the needed calls
    fs_ops_mock_.fsmd_rem_non_evac_frags_ =
        [](std::vector<agg_meta_entry>&, volume_blocks64_t, volume_blocks64_t)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
    };
    fs_ops_mock_.fsmd_add_evac_fragment_ =
        [](const fs_node_key_t&, const range&, array_view<const uint8_t>,
           volume_blocks64_t, agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
        return false;
    };
    fs_ops_mock_.fsmd_fin_flush_commit_ =
        [](volume_blocks64_t, const std::vector<write_transaction>&,
           agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
    };

    reset_agg_wr(data_offset, 0);

    auto exp_key = gen_key("aaa");
    write_transaction wtrans1(exp_key, range{10_KB, 1025_KB});
    write_transaction wtrans2(exp_key, range{2048_KB, 2560_KB});
    write_transaction wtrans3(exp_key, range{8192_KB, 1024_KB});

    auto wbuff                    = make_wbuff('c', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    bool r = agg_wr_->write(wbuff, wtrans1);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans1.written(), 1024_KB);
    BOOST_CHECK_EQUAL(wtrans1.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(10_KB, 1024_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('d', 1_KB, 1_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans1));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(1034_KB, 1_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('e', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    r = agg_wr_->write(wbuff, wtrans2);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans2.written(), 1024_KB);
    BOOST_CHECK_EQUAL(wtrans2.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(2048_KB, 1024_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('i', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    r = agg_wr_->write(wbuff, wtrans2);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans2.written(), 2048_KB);
    BOOST_CHECK_EQUAL(wtrans2.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(3072_KB, 1024_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('j', 512_KB, 512_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans2));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(4096_KB, 512_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    // Now this one should provoke a write IO because there is no space
    // in the aggregation block.
    fs_ops_mock_.curr_disk_offs_   = data_offset.to_bytes();
    wbuff                          = make_wbuff('c', 512_KB, 512_KB);
    allow_add_new_frag_            = true;
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(!r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans3.written(), 0_KB);
    BOOST_CHECK_EQUAL(wtrans3.finished(), false);
    fs_ops_mock_.vmtx_wait_disk_readers_called_ = false;
    fs_ops_mock_.write_can_be_called_ = true;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_commit_disk_write_called_);
    BOOST_REQUIRE(fs_ops_mock_.vmtx_wait_disk_readers_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    // Now the write should succeed. The write block should be flushed and be
    // empty by now.
    auto exp_offs = data_offset + bytes2blocks(agg_write_block_size);

    allow_add_new_frag_            = true;
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans3.written(), 512_KB);
    BOOST_CHECK_EQUAL(wtrans3.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, exp_offs);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(8192_KB, 512_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));

    // Simulate interrupted write
    wbuff                         = make_wbuff('j', 512_KB, 12_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans3));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, exp_offs);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(8704_KB, 12_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);
}

BOOST_AUTO_TEST_CASE(second_lap_read_but_nothing_to_evacuate)
{
    // These two must not be called
    fs_ops_mock_.fsmd_add_evac_fragment_ =
        [](const fs_node_key_t&, const range&, array_view<const uint8_t>,
           volume_blocks64_t, agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
        return false;
    };
    fs_ops_mock_.fsmd_fin_flush_commit_ =
        [](volume_blocks64_t, const std::vector<write_transaction>&,
           agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
    };

    reset_agg_wr(data_offset, 0);

    // Write two fragments and provoke flush disallowing further writing.
    auto exp_key = gen_key("aaa");
    write_transaction wtrans1(exp_key, range{10_KB, 1024_KB});
    write_transaction wtrans2(exp_key, range{2048_KB, 1024_KB});
    write_transaction wtrans3(exp_key, range{8192_KB, 1024_KB});

    auto wbuff                    = make_wbuff('c', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans1));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('d', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans2));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    // Now this one should provoke a write IO because we disallow
    // further writing
    allow_add_new_frag_            = false;
    fs_ops_mock_.curr_disk_offs_   = data_offset.to_bytes();
    wbuff                          = make_wbuff('c', 512_KB, 512_KB);
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    bool r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(!r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    fs_ops_mock_.vmtx_wait_disk_readers_called_ = false;
    fs_ops_mock_.write_can_be_called_ = true;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_commit_disk_write_called_);
    BOOST_REQUIRE(fs_ops_mock_.vmtx_wait_disk_readers_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);

    // Now rest the agg_writer pretending that we start second lap.
    // This should provoke evacuation.
    bool fsmd_rem_non_evac_frags_called = false;
    fs_ops_mock_.fsmd_rem_non_evac_frags_ =
        [&](std::vector<agg_meta_entry>& entries, volume_blocks64_t disk_offset,
            volume_blocks64_t area_size)
    {
        fsmd_rem_non_evac_frags_called = true;
        BOOST_REQUIRE_EQUAL(disk_offset.to_bytes(),
                            data_offset.to_bytes() + agg_write_meta_size);
        BOOST_REQUIRE_EQUAL(area_size.to_bytes(), agg_write_data_size);
        BOOST_REQUIRE_EQUAL(entries.size(), 2);
        BOOST_REQUIRE_EQUAL(to_range(entries[0].rng()), wtrans1.get_range());
        BOOST_REQUIRE_EQUAL(to_range(entries[1].rng()), wtrans2.get_range());
        // No evacuation this time
        entries.clear();
    };

    allow_add_new_frag_ = true;
    reset_agg_wr(data_offset, 1);
    fs_ops_mock_.read_can_be_called_  = true;
    fs_ops_mock_.write_can_be_called_ = false;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_rem_non_evac_frags_called);

    // Confirm that writes after evacuation work
    allow_add_new_frag_            = true;
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans3.written(), 512_KB);
    BOOST_CHECK_EQUAL(wtrans3.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(8192_KB, 512_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
}

BOOST_AUTO_TEST_CASE(second_lap_read_something_to_evacuate)
{
    // These one must not be called
    fs_ops_mock_.fsmd_fin_flush_commit_ =
        [](volume_blocks64_t, const std::vector<write_transaction>&,
           agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
    };

    reset_agg_wr(data_offset, 0);

    // Write two fragments and provoke flush disallowing further writing.
    auto exp_key = gen_key("aaa");
    write_transaction wtrans1(exp_key, range{10_KB, 1024_KB});
    write_transaction wtrans2(exp_key, range{2048_KB, 1024_KB});
    write_transaction wtrans3(exp_key, range{8192_KB, 1024_KB});

    auto wbuff                    = make_wbuff('c', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans1));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('d', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans2));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    // Now this one should provoke a write IO because we disallow
    // further writing
    allow_add_new_frag_            = false;
    fs_ops_mock_.curr_disk_offs_   = data_offset.to_bytes();
    wbuff                          = make_wbuff('c', 512_KB, 512_KB);
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    bool r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(!r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    fs_ops_mock_.vmtx_wait_disk_readers_called_ = false;
    fs_ops_mock_.write_can_be_called_ = true;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_commit_disk_write_called_);
    BOOST_REQUIRE(fs_ops_mock_.vmtx_wait_disk_readers_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);

    // Now rest the agg_writer pretending that we start second lap.
    // This should provoke evacuation.
    bool fsmd_rem_non_evac_frags_called = false;
    fs_ops_mock_.fsmd_rem_non_evac_frags_ =
        [&](std::vector<agg_meta_entry>& entries, volume_blocks64_t disk_offset,
            volume_blocks64_t area_size)
    {
        fsmd_rem_non_evac_frags_called = true;
        BOOST_REQUIRE_EQUAL(disk_offset.to_bytes(),
                            data_offset.to_bytes() + agg_write_meta_size);
        BOOST_REQUIRE_EQUAL(area_size.to_bytes(), agg_write_data_size);
        BOOST_REQUIRE_EQUAL(entries.size(), 2);
        BOOST_REQUIRE_EQUAL(to_range(entries[0].rng()), wtrans1.get_range());
        BOOST_REQUIRE_EQUAL(to_range(entries[1].rng()), wtrans2.get_range());
        // Filter out the first entry
        entries.erase(entries.begin());
    };

    allow_add_new_frag_ = true;
    reset_agg_wr(data_offset, 1);

    fs_ops_mock_.read_can_be_called_  = true;
    fs_ops_mock_.write_can_be_called_ = false;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_rem_non_evac_frags_called);
    // Now if we call again run_one it should execute add_evac_fragment
    fsmd_add_evac_fragment_called_ = false;
    exp_evac_wbuff = make_wbuff('d', 1024_KB, 1024_KB);
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_add_evac_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, wtrans2.fs_node_key());
    BOOST_REQUIRE_EQUAL(given_rng_, wtrans2.get_range());

    // Confirm that writes after evacuation work
    allow_add_new_frag_            = true;
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans3.written(), 512_KB);
    BOOST_CHECK_EQUAL(wtrans3.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(8192_KB, 512_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
}

BOOST_AUTO_TEST_CASE(second_lap_read_all_to_evacuate)
{
    // These one must not be called
    fs_ops_mock_.fsmd_fin_flush_commit_ =
        [](volume_blocks64_t, const std::vector<write_transaction>&,
           agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
    };

    reset_agg_wr(data_offset, 0);

    // Write two fragments and provoke flush disallowing further writing.
    auto exp_key = gen_key("aaa");
    write_transaction wtrans1(exp_key, range{10_KB, 1024_KB});
    write_transaction wtrans2(exp_key, range{2048_KB, 1024_KB});
    write_transaction wtrans3(exp_key, range{8192_KB, 1024_KB});

    auto wbuff                    = make_wbuff('c', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans1));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    wbuff                         = make_wbuff('d', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans2));
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    // Now this one should provoke a write IO because we disallow
    // further writing
    allow_add_new_frag_            = false;
    fs_ops_mock_.curr_disk_offs_   = data_offset.to_bytes();
    wbuff                          = make_wbuff('c', 512_KB, 512_KB);
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    bool r = agg_wr_->write(wbuff, wtrans3);
    BOOST_REQUIRE(!r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    fs_ops_mock_.vmtx_wait_disk_readers_called_ = false;
    fs_ops_mock_.write_can_be_called_ = true;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_commit_disk_write_called_);
    BOOST_REQUIRE(fs_ops_mock_.vmtx_wait_disk_readers_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);

    // Now rest the agg_writer pretending that we start second lap.
    // This should provoke evacuation.
    bool fsmd_rem_non_evac_frags_called = false;
    fs_ops_mock_.fsmd_rem_non_evac_frags_ =
        [&](std::vector<agg_meta_entry>& entries, volume_blocks64_t disk_offset,
            volume_blocks64_t area_size)
    {
        fsmd_rem_non_evac_frags_called = true;
        BOOST_REQUIRE_EQUAL(disk_offset.to_bytes(),
                            data_offset.to_bytes() + agg_write_meta_size);
        BOOST_REQUIRE_EQUAL(area_size.to_bytes(), agg_write_data_size);
        BOOST_REQUIRE_EQUAL(entries.size(), 2);
        BOOST_REQUIRE_EQUAL(to_range(entries[0].rng()), wtrans1.get_range());
        BOOST_REQUIRE_EQUAL(to_range(entries[1].rng()), wtrans2.get_range());
        // Don't filter any entries
    };

    allow_add_new_frag_ = true;
    reset_agg_wr(data_offset, 1);

    fs_ops_mock_.read_can_be_called_  = true;
    fs_ops_mock_.write_can_be_called_ = false;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_rem_non_evac_frags_called);
    // Now if we call again run_one it should execute add_evac_fragment
    // for the first transaction
    fsmd_add_evac_fragment_called_ = false;
    exp_evac_wbuff = make_wbuff('c', 1024_KB, 1024_KB);
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_add_evac_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, wtrans1.fs_node_key());
    BOOST_REQUIRE_EQUAL(given_rng_, wtrans1.get_range());
    // Now if we call again run_one it should execute add_evac_fragment
    // for the first transaction
    fsmd_add_evac_fragment_called_ = false;
    exp_evac_wbuff = make_wbuff('d', 1024_KB, 1024_KB);
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_add_evac_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, wtrans2.fs_node_key());
    BOOST_REQUIRE_EQUAL(given_rng_, wtrans2.get_range());

    // Confirm that writes after evacuation work.
    // However, we have evacuated all fragments thus we shouldn't have space
    // left. So disable adding new fragment.
    allow_add_new_frag_            = false;
    fs_ops_mock_.curr_disk_offs_   = data_offset.to_bytes();
    wbuff                          = make_wbuff('r', 512_KB, 512_KB);
    fsmd_add_new_fragment_called_  = false;
    fsmd_commit_disk_write_called_ = false;
    agg_wr_->final_write(std::move(wbuff), std::move(wtrans3));
    fs_ops_mock_.vmtx_wait_disk_readers_called_ = false;
    fs_ops_mock_.write_can_be_called_ = true;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_commit_disk_write_called_);
    BOOST_REQUIRE(fs_ops_mock_.vmtx_wait_disk_readers_called_);
    //  After that, the metadata of the next write block will be read.
    //  It'll be the same. Just filter it out.
    auto exp_offs = data_offset + bytes2blocks(agg_write_block_size);

    fs_ops_mock_.curr_disk_offs_   = exp_offs.to_bytes();
    fsmd_rem_non_evac_frags_called = false;
    fs_ops_mock_.fsmd_rem_non_evac_frags_ =
        [&](std::vector<agg_meta_entry>& entries, volume_blocks64_t disk_offset,
            volume_blocks64_t area_size)
    {
        fsmd_rem_non_evac_frags_called = true;
        BOOST_REQUIRE_EQUAL(disk_offset.to_bytes(),
                            exp_offs.to_bytes() + agg_write_meta_size);
        BOOST_REQUIRE_EQUAL(area_size.to_bytes(), agg_write_data_size);
        BOOST_REQUIRE_EQUAL(entries.size(), 2);
        BOOST_REQUIRE_EQUAL(to_range(entries[0].rng()), wtrans1.get_range());
        BOOST_REQUIRE_EQUAL(to_range(entries[1].rng()), wtrans2.get_range());
        // Filter out all entries
        entries.clear();
    };
    allow_add_new_frag_               = true;
    fsmd_add_new_fragment_called_     = false;
    fs_ops_mock_.read_can_be_called_  = true;
    fs_ops_mock_.write_can_be_called_ = false;
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(fsmd_rem_non_evac_frags_called);
    BOOST_REQUIRE(fsmd_rem_non_evac_frags_called);
    // After the evacuation is done the pending write must be written
    // automatically
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_REQUIRE_EQUAL(given_offs_, exp_offs);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(8192_KB, 512_KB, frag_rng));
}

BOOST_AUTO_TEST_CASE(flush_on_stop)
{
    fs_ops_mock_.fsmd_rem_non_evac_frags_ =
        [](std::vector<agg_meta_entry>&, volume_blocks64_t, volume_blocks64_t)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
    };
    fs_ops_mock_.fsmd_add_evac_fragment_ =
        [](const fs_node_key_t&, const range&, array_view<const uint8_t>,
           volume_blocks64_t, agg_wblock_sync_t&)
    {
        BOOST_REQUIRE_MESSAGE(false, "Must not be called in this test");
        return false;
    };
    reset_agg_wr(data_offset, 0);

    auto exp_key = gen_key("aaa");
    write_transaction wtrans1(exp_key, range{10_KB, 1025_KB});

    auto wbuff                    = make_wbuff('c', 1024_KB, 1024_KB);
    allow_add_new_frag_           = true;
    fsmd_add_new_fragment_called_ = false;
    bool r = agg_wr_->write(wbuff, wtrans1);
    BOOST_REQUIRE(r);
    BOOST_REQUIRE(fsmd_add_new_fragment_called_);
    BOOST_CHECK_EQUAL(wtrans1.written(), 1024_KB);
    BOOST_CHECK_EQUAL(wtrans1.finished(), false);
    BOOST_REQUIRE_EQUAL(given_offs_, data_offset);
    BOOST_REQUIRE_EQUAL(given_key_, exp_key);
    BOOST_REQUIRE_EQUAL(given_rng_, range(10_KB, 1024_KB, frag_rng));
    BOOST_REQUIRE(given_frag_.size() >= wbuff.size()); // Rounded up
    BOOST_REQUIRE(0 ==
                  ::memcmp(given_frag_.data(), wbuff.data(), wbuff.size()));
    // The agg write block is not full, so nothing must happen here.
    fs_ops_mock_.run_one();
    BOOST_REQUIRE(!fsmd_commit_disk_write_called_);

    bool fsmd_fin_flush_commit_called = false;
    fs_ops_mock_.fsmd_fin_flush_commit_ =
        [&](volume_blocks64_t disk_offset,
            const std::vector<write_transaction>& wtranss,
            agg_wblock_sync_t& wblock)
    {
        fsmd_fin_flush_commit_called = true;
        BOOST_REQUIRE_EQUAL(disk_offset, data_offset);
        BOOST_CHECK(wtranss.empty());
        // Just for the check
        auto ent = wblock->end_disk_write();
        BOOST_REQUIRE_EQUAL(ent.size(), 1);
        BOOST_REQUIRE_EQUAL(ent[0].key(), wtrans1.fs_node_key());
        BOOST_REQUIRE_EQUAL(to_range(ent[0].rng()), (range{10_KB, 1024_KB}));
    };
    agg_wr_->stop_flush();
    BOOST_REQUIRE(fsmd_fin_flush_commit_called);
}

BOOST_AUTO_TEST_SUITE_END()
