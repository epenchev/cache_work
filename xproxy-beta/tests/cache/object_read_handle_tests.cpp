#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "cache_fs_ops_empty.h"
#include "../../cache/object_read_handle.h"
#include "../../cache/cache_error.h"
#include "../../cache/object_frag_hdr.h"
#include "../../cache/range_elem.h"

using namespace cache::detail;

namespace
{
read_transaction make_rtrans(bytes64_t offs, bytes32_t size) noexcept
{
    return read_transaction(
        object_key(fs_node_key_t{"aaa", 3}, range{offs, size}));
}

range_elem make_relem(bytes64_t offs, bytes32_t size) noexcept
{
    return make_range_elem(
        offs, size, volume_blocks64_t::round_up_to_blocks(128_KB + offs));
}

////////////////////////////////////////////////////////////////////////////////

// A helper class to test the object_write_handle functionality
class fs_ops_impl final : public cache_fs_ops_empty
{
    io_service_t ios_;
    io_service_t::work iow_{ios_};

    range_elem curr_rng_;
    std::vector<uint8_t> buff_;

    std::vector<uint8_t> all_data_;

public:
    read_transaction rtrans_;

    bool find_rng_        = true;
    bool find_aggw_rng_   = false;
    bool lock_volume_mtx_ = false;
    bool service_stopped_ = false;

    bool find_next_rng_called_      = false;
    bool try_read_agg_frag_called_  = false;
    bool vmtx_lock_shared_called_   = false;
    bool vmtx_unlock_shared_called_ = false;
    bool fmsd_end_read_called_      = false;

    fs_node_key_t key_;

public:
    fs_ops_impl() noexcept {}
    ~fs_ops_impl() noexcept final { rtrans_.invalidate(); }

    void run_one() noexcept { ios_.poll_one(); }

    void reset_state() noexcept
    {
        find_next_rng_called_      = false;
        try_read_agg_frag_called_  = false;
        vmtx_lock_shared_called_   = false;
        vmtx_unlock_shared_called_ = false;
        fmsd_end_read_called_      = false;
    }

    void set_curr_buff(bytes64_t rng_offs, bytes32_t rng_size, uint8_t fill,
                       bool rng_in_memory = false,
                       bool corrupt_hdr = false) noexcept
    {
        update_all_data();

        curr_rng_.set_rng_offset(rng_offs);
        curr_rng_.set_rng_size(rng_size);
        curr_rng_.set_disk_offset(
            volume_blocks64_t::round_up_to_blocks(rng_offs + 128_KB));
        curr_rng_.set_in_memory(rng_in_memory);
        const auto exp_hdr = object_frag_hdr::create(key_, curr_rng_);
        buff_.resize(object_frag_size(rng_size), fill);
        if (corrupt_hdr)
            ::memset(buff_.data(), 0, sizeof(exp_hdr));
        else
            ::memcpy(buff_.data(), &exp_hdr, sizeof(exp_hdr));
    }

    void update_all_data() noexcept
    {
        if (!buff_.empty())
        {
            const auto sz = all_data_.size();
            all_data_.resize(sz + curr_rng_.rng_size());
            ::memcpy(all_data_.data() + sz,
                     buff_.data() + sizeof(object_frag_hdr),
                     curr_rng_.rng_size());
        }
    }

    const std::vector<uint8_t>& all_data() const noexcept { return all_data_; }

private:
    void aios_push_read_queue(owner_ptr_t<aio_task> t) noexcept final
    {
        if (service_stopped_)
        {
            t->service_stopped();
            return;
        }
        ios_.post(
            [this, t]
            {
                BOOST_REQUIRE_MESSAGE(t->operation() == aio_op::read,
                                      "Must do only read operations");
                if (auto d = t->on_begin_io_op())
                {
                    err_code_t err; // Won't test cache errors
                    BOOST_REQUIRE_MESSAGE(
                        d->size_ == object_frag_size(curr_rng_.rng_size()),
                        "Wrong read size");
                    BOOST_REQUIRE_MESSAGE(
                        d->offs_ == curr_rng_.disk_offset().to_bytes(),
                        "Wrong read offset");
                    X3ME_ASSERT(buff_.size() == d->size_,
                                "Wrong size of the buff");
                    ::memcpy(d->buf_, buff_.data(), buff_.size());
                    t->on_end_io_op(err);
                }
            });
    }
    void aios_enqueue_read_queue(owner_ptr_t<aio_task> t) noexcept final
    {
        aios_push_read_queue(t);
    }
    bool vmtx_lock_shared(bytes64_t) noexcept final
    {
        vmtx_lock_shared_called_ = true;
        return lock_volume_mtx_;
    }
    void vmtx_unlock_shared() noexcept final
    {
        vmtx_unlock_shared_called_ = true;
        BOOST_REQUIRE_MESSAGE(lock_volume_mtx_,
                              "Must not be called if it's not locked");
    }
    void fsmd_end_read(read_transaction&& rtrans) noexcept final
    {
        rtrans_               = std::move(rtrans);
        fmsd_end_read_called_ = true;
    }
    expected_t<range_elem, err_code_t>
    fsmd_find_next_range_elem(const read_transaction& rtrans) noexcept final
    {
        find_next_rng_called_ = true;
        if (find_rng_)
        {
            X3ME_ASSERT(rtrans.curr_offset() >= curr_rng_.rng_offset(),
                        "Invalid current range");
            return curr_rng_;
        }
        return boost::make_unexpected(err_code_t{
            cache::internal_logic_error, cache::get_cache_error_category()});
    }
    bool aggw_try_read_frag(const fs_node_key_t&, const range_elem& rng,
                            frag_buff_t buf) noexcept final
    {
        try_read_agg_frag_called_ = true;
        if (find_aggw_rng_)
        {
            X3ME_ASSERT(rng == curr_rng_, "Invalid current range");
            X3ME_ASSERT(object_frag_size(buf.size()) == buff_.size(),
                        "Invalid buff size");
            ::memcpy(buf.data(), buff_.data(), buff_.size());
        }
        return find_aggw_rng_;
    }
};

////////////////////////////////////////////////////////////////////////////////

class fixture
{
public:
    std::shared_ptr<fs_ops_impl> fs_ops_ = std::make_shared<fs_ops_impl>();

private:
    bytes32_t single_buff_size_ = 2_KB;

    object_rhandle_ptr_t handle_;

    std::vector<std::vector<char>> buffs_;

public:
    void init(bytes64_t offs, bytes64_t size) noexcept
    {
        fs_ops_->key_ = fs_node_key_t{"aaa", 3};
        read_transaction rtrans(object_key(fs_ops_->key_, range{offs, size}));
        handle_ = new object_read_handle(fs_ops_, std::move(rtrans));
    }

    template <typename Handler>
    void async_read(uint32_t num_buffers, Handler&& h) noexcept
    {
        cache::const_buffers buffs;

        auto start_idx = buffs_.size();
        buffs_.resize(buffs_.size() + num_buffers);
        for (auto i = 0U; i < num_buffers; ++i)
        {
            auto& buff = buffs_[start_idx + i];
            buff.resize(single_buff_size_);

            buffs.emplace_back(buff.data(), buff.size());
        }

        handle_->async_read(std::move(buffs), std::move(h));
    }

    void service_stopped() noexcept
    {
        fs_ops_->service_stopped_ = true;
        static_cast<aio_task*>(handle_.get())->service_stopped();
    }

    void close_handle() noexcept { handle_->close(); }

    bool same_buffers(bytes32_t skipb, bytes32_t skipe,
                      bytes32_t rem_buffs = 0) const noexcept
    {
        assert((skipb % single_buff_size_) == 0);
        assert((skipe % single_buff_size_) == 0);

        const auto& all_data = fs_ops_->all_data();

        assert(skipb + skipe < all_data.size());

        auto all = x3me::mem_utils::make_array_view(
            all_data.data() + skipb, all_data.size() - (skipb + skipe));

        assert(rem_buffs <= buffs_.size());
        auto buffs = x3me::mem_utils::make_array_view(
            buffs_.data(), buffs_.size() - rem_buffs);

        if ((buffs.size() * single_buff_size_) != all.size())
            return false;
        bytes32_t offs = 0;
        for (const auto& b : buffs)
        {
            if (::memcmp(all.data() + offs, b.data(), b.size()) != 0)
                return false;
            offs += b.size();
        }
        return true;
    }
};

} // namespace
////////////////////////////////////////////////////////////////////////////////

// We test the functionality of the object_read_handle here, but we don't
// test its behavior in the multithreading environment, because it can't be
// reliably tested (or at least I can't figure out a way to do it).
BOOST_FIXTURE_TEST_SUITE(object_read_handle_tests, fixture)

BOOST_AUTO_TEST_CASE(close_no_read)
{
    init(16_KB, min_obj_size);

    close_handle();
    fs_ops_->run_one();

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 16_KB);
}

BOOST_AUTO_TEST_CASE(service_stopped_no_read)
{
    init(16_KB, min_obj_size);

    service_stopped();
    close_handle();
    fs_ops_->run_one();

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Even this is not called
    BOOST_CHECK(!fs_ops_->rtrans_.valid());
}

BOOST_AUTO_TEST_CASE(single_read_no_skip_beg_end)
{
    init(16_KB, min_obj_size);

    fs_ops_->set_curr_buff(16_KB, min_obj_size, 'a');

    bool handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // We don't lock
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // This is automatically called

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 16_KB + min_obj_size);
    BOOST_CHECK(fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    BOOST_CHECK(same_buffers(0, 0));
}

BOOST_AUTO_TEST_CASE(single_read_no_skip_beg_end_found_in_agg_buff)
{
    init(16_KB, min_obj_size);

    fs_ops_->set_curr_buff(16_KB, min_obj_size, 'a', true /*in memory*/);
    fs_ops_->find_aggw_rng_ = true;

    bool handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // This is skipped
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // This is automatically called

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 16_KB + min_obj_size);
    BOOST_CHECK(fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    BOOST_CHECK(same_buffers(0, 0));
}

BOOST_AUTO_TEST_CASE(calc_copy_rng_equal_rng_elem_and_rtrans)
{
    const auto r = object_read_handle::calc_copy_rng(make_rtrans(16_KB, 32_KB),
                                                     make_relem(16_KB, 32_KB));
    BOOST_CHECK_EQUAL(r.first, 0); // No offset to skip
    BOOST_CHECK_EQUAL(r.second, 32_KB);
}

BOOST_AUTO_TEST_CASE(calc_copy_rng_rng_elem_smaller_than_rtrans)
{
    const auto r = object_read_handle::calc_copy_rng(make_rtrans(16_KB, 32_KB),
                                                     make_relem(16_KB, 16_KB));
    BOOST_CHECK_EQUAL(r.first, 0); // No offset to skip
    BOOST_CHECK_EQUAL(r.second, 16_KB);
}

BOOST_AUTO_TEST_CASE(calc_copy_rng_rng_elem_bigger_than_rtrans)
{
    const auto r = object_read_handle::calc_copy_rng(make_rtrans(16_KB, 16_KB),
                                                     make_relem(16_KB, 32_KB));
    BOOST_CHECK_EQUAL(r.first, 0); // No skip offset
    BOOST_CHECK_EQUAL(r.second, 16_KB);
}

BOOST_AUTO_TEST_CASE(calc_copy_rng_rng_elem_smaller_than_rtrans_and_offset)
{
    const auto r = object_read_handle::calc_copy_rng(make_rtrans(24_KB, 48_KB),
                                                     make_relem(20_KB, 32_KB));
    BOOST_CHECK_EQUAL(r.first, 4_KB); // Skip 4_KB
    BOOST_CHECK_EQUAL(r.second, 28_KB); // Range_elem length - skipped
}

BOOST_AUTO_TEST_CASE(calc_copy_rng_rng_elem_bigger_than_rtrans_and_offset)
{
    const auto r = object_read_handle::calc_copy_rng(make_rtrans(24_KB, 30_KB),
                                                     make_relem(20_KB, 48_KB));
    BOOST_CHECK_EQUAL(r.first, 4_KB); // Skip 4_KB
    BOOST_CHECK_EQUAL(r.second, 30_KB); // Trans length
}

BOOST_AUTO_TEST_CASE(calc_copy_rng_rtrans_at_end_of_rng_elem)
{
    const auto r = object_read_handle::calc_copy_rng(make_rtrans(40_KB, 8_KB),
                                                     make_relem(20_KB, 48_KB));
    BOOST_CHECK_EQUAL(r.first, 20_KB); // Skip 20
    BOOST_CHECK_EQUAL(r.second, 8_KB); // Get only the read transaction length
}

BOOST_AUTO_TEST_CASE(disk_and_memory_read_skip_beg_and_end)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(16_KB, 3 * min_obj_size, 'a', false /*from disk*/);

    bool handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // Called before disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_); // It's been found in the mem
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // Now it's finished

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB + 2 * min_obj_size);
    BOOST_CHECK(fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    BOOST_CHECK(same_buffers(4_KB, 4_KB));
}

BOOST_AUTO_TEST_CASE(agg_buffer_and_mulitple_memory_reads)
{
    init(20_KB, 3 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 3 * min_obj_size, 'b', true /*from memor*/);
    fs_ops_->find_aggw_rng_ = true;

    bool handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // Now it's finished

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB + 3 * min_obj_size);
    BOOST_CHECK(fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    BOOST_CHECK(same_buffers(0_KB, 0_KB));
}

BOOST_AUTO_TEST_CASE(few_disk_reads_skip_beg_end)
{
    init(20_KB, 3 * min_obj_size + 4_KB);

    auto next_beg_offs = 16_KB + min_obj_size + 4_KB + 2_KB;
    // First read ////////////////////////////////////////
    fs_ops_->set_curr_buff(16_KB, next_beg_offs - 16_KB, 'a',
                           true /*from memory, but won't be found*/);
    fs_ops_->lock_volume_mtx_ = true;

    bool handler_called = false;
    async_read(5, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size + 2_KB);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // Disk read
    BOOST_CHECK(fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    // Second read ////////////////////////////////////////
    fs_ops_->reset_state();
    fs_ops_->lock_volume_mtx_ = true;
    fs_ops_->set_curr_buff(next_beg_offs, min_obj_size, 'c',
                           true /*from memory, but won't be found*/);
    next_beg_offs += min_obj_size;

    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // Disk read
    BOOST_CHECK(fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    // Last read ////////////////////////////////////////
    fs_ops_->reset_state();
    fs_ops_->lock_volume_mtx_ = true;
    fs_ops_->set_curr_buff(next_beg_offs, min_obj_size + 6_KB, 'c',
                           true /*from memory, but won't be found*/);

    handler_called = false;
    async_read(5, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size + 2_KB);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // Disk read
    BOOST_CHECK(fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // Finished

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    BOOST_CHECK(same_buffers(4_KB, 4_KB));
}

BOOST_AUTO_TEST_CASE(disk_memory_and_agg_read_skip_end)
{
    init(20_KB, 3 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', true /*from memor*/);

    bool handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // We have disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_); // Everything is in frag buff
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    // Last fragment will be found in the agg buffer and something won't be read
    fs_ops_->set_curr_buff(20_KB + 2 * min_obj_size, 2 * min_obj_size, 'e',
                           true /*from memor*/);
    fs_ops_->find_aggw_rng_ = true;

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // Now it's finished

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB + 3 * min_obj_size);
    BOOST_CHECK(fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    BOOST_CHECK(same_buffers(0_KB, min_obj_size));
}

BOOST_AUTO_TEST_CASE(error_eof_on_read_after_eof)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', true /*from memor*/);

    bool handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // We have disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, min_obj_size);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_); // Everything is in frag buff
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // Now it's finished

    // Reading after EOF should get us another EOF
    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::eof);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_); // Everything is in frag buff
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Now it's finished

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB + 2 * min_obj_size);
    BOOST_CHECK(fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    // Remove last 4 buffers allocated for the second EOF
    BOOST_CHECK(same_buffers(0_KB, 0_KB, 4));
}

BOOST_AUTO_TEST_CASE(error_on_missing_range_elem)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(16_KB, 2 * min_obj_size, 'c', true /*from memor*/);

    bool handler_called = false;
    async_read(6, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, 2 * min_obj_size - 4_KB);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // We have disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->find_rng_ = false; // Now we won't find the next range

    fs_ops_->reset_state();
    handler_called = false;
    async_read(4, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::internal_logic_error);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_); // This one fails
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Everything is frag buff
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // No disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_); // Skipped too
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_); // End read is called

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // It's already called
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(),
                      20_KB + 2 * min_obj_size - 4_KB);
    BOOST_CHECK(!fs_ops_->rtrans_.finished());

    fs_ops_->update_all_data(); // We need it to compare buffers to all data
    // Remove the 4 buffers provided to internal_logic_error
    BOOST_CHECK(same_buffers(4_KB, 0_KB, 4));
}

BOOST_AUTO_TEST_CASE(no_error_zero_bytes_when_null_buffers)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', true /*from memor*/);

    bool handler_called = false;
    async_read(0, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK(!err);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    // We'll do disk read just because our test driver will return
    // a 'hardcoded' range elem.
    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // We have disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_); // Still not finished

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB);
    BOOST_CHECK(!fs_ops_->rtrans_.finished());
}

BOOST_AUTO_TEST_CASE(error_on_wrong_hdr_from_agg_buffer)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', true /*from memor*/,
                           true /*corrupt_hdr*/);
    fs_ops_->find_aggw_rng_ = true;

    bool handler_called = false;
    async_read(0, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::corrupted_object_data);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_); // We fail before this
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB);
    BOOST_CHECK(!fs_ops_->rtrans_.finished());
}

BOOST_AUTO_TEST_CASE(error_on_wrong_hdr_from_disk)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', false /*from disk*/,
                           true /*corrupt_hdr*/);

    bool handler_called = false;
    async_read(0, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::corrupted_object_data);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_); // Loaded from disk
    BOOST_CHECK(fs_ops_->vmtx_lock_shared_called_); // We did disk read
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB);
    BOOST_CHECK(!fs_ops_->rtrans_.finished());
}

BOOST_AUTO_TEST_CASE(pending_read_and_close)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', false /*from disk*/,
                           true /*corrupt_hdr*/);

    bool handler_called = false;
    async_read(0, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::operation_aborted);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    close_handle();
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK_EQUAL(fs_ops_->rtrans_.curr_offset(), 20_KB);
    BOOST_CHECK(!fs_ops_->rtrans_.finished());
}

BOOST_AUTO_TEST_CASE(pending_read_and_service_stopped)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', false /*from disk*/,
                           true /*corrupt_hdr*/);

    bool handler_called = false;
    async_read(0, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::service_stopped);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    service_stopped();
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK(!fs_ops_->rtrans_.valid());
    BOOST_CHECK(!fs_ops_->rtrans_.finished());
}

BOOST_AUTO_TEST_CASE(service_stopped_and_read_attempt)
{
    init(20_KB, 2 * min_obj_size);

    fs_ops_->set_curr_buff(20_KB, 2 * min_obj_size, 'c', false /*from disk*/,
                           true /*corrupt_hdr*/);

    service_stopped();

    bool handler_called = false;
    async_read(0, [&](err_code_t err, bytes32_t read_bytes)
               {
                   handler_called = true;
                   BOOST_CHECK_EQUAL(err.value(), cache::service_stopped);
                   BOOST_CHECK_EQUAL(read_bytes, 0);
               });
    BOOST_REQUIRE(handler_called);

    BOOST_CHECK(!fs_ops_->find_next_rng_called_);
    BOOST_CHECK(!fs_ops_->try_read_agg_frag_called_);
    BOOST_CHECK(!fs_ops_->vmtx_lock_shared_called_);
    BOOST_CHECK(!fs_ops_->vmtx_unlock_shared_called_);
    BOOST_CHECK(!fs_ops_->fmsd_end_read_called_);
    BOOST_CHECK(!fs_ops_->rtrans_.valid());
    BOOST_CHECK(!fs_ops_->rtrans_.finished());
}

BOOST_AUTO_TEST_SUITE_END()
