#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "cache_fs_ops_empty.h"
#include "../../cache/object_write_handle.h"
#include "../../cache/cache_error.h"
#include "../../cache/range_elem.h"

using namespace cache::detail;

namespace
{
// A helper class to test the object_write_handle functionality
class fs_ops_impl final : public cache_fs_ops_empty
{
    io_service_t ios_;
    io_service_t::work iow_{ios_};

    write_transaction wtrans_;

    std::string all_data_;
    bytes32_t last_frag_size_         = 0;
    bool agg_write_frag_called_       = false;
    bool agg_write_final_frag_called_ = false;
    bool accept_write_                = false;
    bool service_stopped_             = false;

public:
    ~fs_ops_impl() noexcept final { wtrans_.invalidate(); }
    void set_accept_write(bool v) noexcept { accept_write_ = v; }
    void set_service_stopped() noexcept { service_stopped_ = true; }

    void reset_state() noexcept
    {
        last_frag_size_              = 0;
        agg_write_frag_called_       = false;
        agg_write_final_frag_called_ = false;
    }

    void run_one() noexcept { ios_.run_one(); }

    const auto& wtrans() const noexcept { return wtrans_; }

    const std::string& all_data() const noexcept { return all_data_; }

    auto last_frag_size() const noexcept { return last_frag_size_; }
    auto agg_write_frag_called() const noexcept
    {
        return agg_write_frag_called_;
    }
    auto agg_write_final_frag_called() const noexcept
    {
        return agg_write_final_frag_called_;
    }

private:
    void aios_push_write_queue(owner_ptr_t<aio_task> t) noexcept final
    {
        if (service_stopped_)
        {
            t->service_stopped();
            return;
        }
        ios_.post([t]
                  {
                      t->exec();
                  });
    }

    void aios_enqueue_write_queue(owner_ptr_t<aio_task> t) noexcept final
    {
        if (service_stopped_)
        {
            t->service_stopped();
            return;
        }
        ios_.post([t]
                  {
                      t->exec();
                  });
    }

    bool aggw_write_frag(const frag_write_buff& buff,
                         write_transaction& trans) noexcept final
    {
        if (accept_write_)
        {
            trans.inc_written(buff.size());
            all_data_.append((const char*)buff.data(), buff.size());
            last_frag_size_ = buff.size();
        }
        agg_write_frag_called_ = true;
        return accept_write_;
    }

    void aggw_write_final_frag(frag_write_buff&& buff,
                               write_transaction&& trans) noexcept final
    {
        trans.inc_written(buff.size());
        wtrans_ = std::move(trans);
        all_data_.append((const char*)buff.data(), buff.size());
        last_frag_size_ = buff.size();
        buff.clear(); // Simulate consumption
        agg_write_final_frag_called_ = true;
    }
};

class fixture
{
public:
    std::shared_ptr<fs_ops_impl> fs_ops_ = std::make_shared<fs_ops_impl>();

private:
    bytes32_t single_buff_size_ = 1_KB;

    object_whandle_ptr_t handle_;

    std::vector<std::vector<char>> buffs_;

public:
    void init(const range& actual_range, const range& trans_range) noexcept
    {
        write_transaction wtrans{fs_node_key_t{"aaa", 3}, trans_range};
        handle_ =
            new object_write_handle(fs_ops_, actual_range, std::move(wtrans));
    }
    void set_single_buff_size(bytes32_t size) noexcept
    {
        single_buff_size_ = size;
    }

    template <typename Handler>
    void async_write(uint32_t num_buffers, Handler&& h) noexcept
    {
        cache::const_buffers buffs;

        uint8_t c      = 'a';
        auto start_idx = buffs_.size();
        buffs_.resize(buffs_.size() + num_buffers);
        for (auto i = 0U; i < num_buffers; ++i, ++c)
        {
            auto& buff = buffs_[start_idx + i];
            buff.resize(single_buff_size_, c);

            buffs.emplace_back(buff.data(), buff.size());
        }

        handle_->async_write(std::move(buffs), std::move(h));
    }

    void service_stopped() noexcept
    {
        fs_ops_->set_service_stopped();
        static_cast<aio_task*>(handle_.get())->service_stopped();
    }

    void close_handle() noexcept { handle_->close(); }

    bool same_buffers(bytes32_t skip_beg, bytes32_t skip_end) const noexcept
    {
        assert((skip_beg % single_buff_size_) == 0);
        assert((skip_end % single_buff_size_) == 0);
        const auto skipb = skip_beg / single_buff_size_;
        const auto skipe = skip_end / single_buff_size_;
        assert(skipb + skipe < buffs_.size());

        auto buffs = x3me::mem_utils::make_array_view(
            buffs_.data() + skipb, buffs_.size() - (skipb + skipe));

        const auto& all_data = fs_ops_->all_data();
        if ((buffs.size() * single_buff_size_) != all_data.size())
            return false;
        bytes32_t offs = 0;
        for (const auto& b : buffs)
        {
            if (::memcmp(all_data.data() + offs, b.data(), b.size()) != 0)
                return false;
            offs += b.size();
        }
        return true;
    }
};

} // namespace
////////////////////////////////////////////////////////////////////////////////

// We test the functionality of the object_write_handle here, but we don't
// test its behavior in the multithreading environment, because it can't be
// reliably tested (or at least I can't figure out a way to do it).
BOOST_FIXTURE_TEST_SUITE(object_write_handle_tests, fixture)

BOOST_AUTO_TEST_CASE(single_write_close_no_final_frag)
{
    const auto skip_beg = 1_KB;
    const auto skip_end = 1_KB;
    const range act_rng{0_KB, 10_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->reset_state();
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 8_KB);
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len());
    BOOST_CHECK(fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end));
}

BOOST_AUTO_TEST_CASE(multiple_user_writes_close_no_final_frag)
{
    const auto skip_beg = 2_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), trn_rng.len());
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len());
    BOOST_CHECK(fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end));
}

BOOST_AUTO_TEST_CASE(writes_unexpected_data)
{
    const auto skip_beg = 0_KB;
    const auto skip_end = 2_KB;
    const range act_rng{0_KB, 10_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), trn_rng.len());
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(1, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK_EQUAL(err.value(), cache::unexpected_data);
                    BOOST_CHECK_EQUAL(written, 0);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len());
    BOOST_CHECK(fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end + 1_KB));
}

BOOST_AUTO_TEST_CASE(write_more_than_max_frag_size)
{
    const auto skip_beg = 0_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 1088_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    set_single_buff_size(64_KB);
    fs_ops_->set_accept_write(true);

    fs_ops_->reset_state();
    for (int i = 0; i < 7; ++i)
    {
        bool handler_called = false;
        async_write(2, [&](err_code_t err, bytes32_t written)
                    {
                        handler_called = true;
                        BOOST_CHECK(!err);
                        BOOST_CHECK_EQUAL(written, 128_KB);
                        BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                        BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                    });
        fs_ops_->run_one();
        BOOST_REQUIRE(handler_called);
    }
    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(2, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 128_KB);
                    // One fragment should be filled now and a call
                    // to agw_write_frag should have been made.
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    // The last write. The agg_write_frag should be called again.
    handler_called = false;
    fs_ops_->reset_state();
    async_write(1, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 64_KB);
                    // One fragment should be filled now and a call
                    // to agw_write_frag should have been made.
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len());
    BOOST_CHECK(fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end));
}

BOOST_AUTO_TEST_CASE(no_writes_close)
{
    const auto skip_beg = 0_KB;
    const auto skip_end = 2_KB;
    const range act_rng{0_KB, 10_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), 0);
    BOOST_CHECK(!fs_ops_->wtrans().finished());
}

BOOST_AUTO_TEST_CASE(premature_close)
{
    const auto skip_beg = 2_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    // Now close before the last chunk is written.
    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();

    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), trn_rng.len() - 10_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len() - 10_KB);
    BOOST_CHECK(!fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end));
}

BOOST_AUTO_TEST_CASE(close_abort_pending_write)
{
    const auto skip_beg = 0_KB;
    const auto skip_end = 2_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK_EQUAL(err.value(), cache::operation_aborted);
                    BOOST_CHECK_EQUAL(written, 0);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    // Now close and abort the previous pending write
    close_handle();
    BOOST_REQUIRE(handler_called);

    fs_ops_->run_one();
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 10_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), 10_KB);
    BOOST_CHECK(!fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, 10_KB));
}

BOOST_AUTO_TEST_CASE(repeat_write_on_agg_writer_busy)
{
    const auto skip_beg = 0_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 1152_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    set_single_buff_size(64_KB);
    fs_ops_->set_accept_write(true);

    fs_ops_->reset_state();
    for (int i = 0; i < 7; ++i)
    {
        bool handler_called = false;
        async_write(2, [&](err_code_t err, bytes32_t written)
                    {
                        handler_called = true;
                        BOOST_CHECK(!err);
                        BOOST_CHECK_EQUAL(written, 128_KB);
                        BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                        BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                    });
        fs_ops_->run_one();
        BOOST_REQUIRE(handler_called);
    }
    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(3, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 192_KB);
                    // One fragment should be filled now and a call
                    // to agw_write_frag should have been made.
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    // Simulate that the aggregate writer fails to write and then repeat.
    fs_ops_->set_accept_write(false);
    fs_ops_->run_one();
    BOOST_REQUIRE(!handler_called);
    fs_ops_->set_accept_write(true);
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    // The last write. The agg_write_frag should be called again.
    handler_called = false;
    fs_ops_->reset_state();
    async_write(1, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 64_KB);
                    // One fragment should be filled now and a call
                    // to agw_write_frag should have been made.
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len());
    BOOST_CHECK(fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end));
}

BOOST_AUTO_TEST_CASE(make_last_write_final_on_agg_writer_busy)
{
    const auto skip_beg = 2_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0_KB);
                    BOOST_CHECK(fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    // The last write will be postponed and become a final one
    fs_ops_->set_accept_write(false);
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);

    fs_ops_->reset_state();
    close_handle();
    fs_ops_->run_one();
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), trn_rng.len());
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().written(), trn_rng.len());
    BOOST_CHECK(fs_ops_->wtrans().finished());

    BOOST_CHECK(same_buffers(skip_beg, skip_end));
}

BOOST_AUTO_TEST_CASE(pending_write_and_service_stopped)
{
    const auto skip_beg = 2_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK_EQUAL(err.value(), cache::service_stopped);
                    BOOST_CHECK_EQUAL(written, 0);
                });
    // This will provoke calling the user handler
    service_stopped();
    BOOST_REQUIRE(handler_called);

    // Then the user calls close
    fs_ops_->reset_state();
    close_handle();
    // fs_ops_->run_one(); Don't call this, the service is stopped
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().valid(), false);
    BOOST_CHECK(!fs_ops_->wtrans().finished());
}

BOOST_AUTO_TEST_CASE(pending_close_and_service_stopped)
{
    const auto skip_beg = 2_KB;
    const auto skip_end = 0_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK_EQUAL(err.value(), cache::operation_aborted);
                    BOOST_CHECK_EQUAL(written, 0);
                });
    close_handle();
    // Stopping the service will prevent the final write from happening
    service_stopped();
    BOOST_REQUIRE(handler_called);

    // Then the user calls close, by mistake but this is no-op, because the
    // service is no longer running.
    close_handle();
    // fs_ops_->run_one(); Don't call this, the service is stopped
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().valid(), false);
    BOOST_CHECK(!fs_ops_->wtrans().finished());
}

BOOST_AUTO_TEST_CASE(service_stopped_and_write)
{
    const auto skip_beg = 2_KB;
    const auto skip_end = 1_KB;
    const range act_rng{0_KB, 30_KB};
    const range trn_rng{act_rng.beg() + skip_beg,
                        act_rng.len() - (skip_beg + skip_end)};

    init(act_rng, trn_rng);
    fs_ops_->set_accept_write(true);

    bool handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK(!err);
                    BOOST_CHECK_EQUAL(written, 10_KB);
                    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
                    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
                });
    fs_ops_->run_one();
    BOOST_REQUIRE(handler_called);
    // Stopping the service will prevent the final write from happening
    service_stopped();
    handler_called = false;
    fs_ops_->reset_state();
    async_write(10, [&](err_code_t err, bytes32_t written)
                {
                    handler_called = true;
                    BOOST_CHECK_EQUAL(err.value(), cache::service_stopped);
                    BOOST_CHECK_EQUAL(written, 0);
                });
    // The handler is invoked immediately because the service is stopped.
    BOOST_REQUIRE(handler_called);

    // Then the user calls close, by mistake but this is no-op, because the
    // service is no longer running.
    close_handle();
    // fs_ops_->run_one(); Don't call this, the service is stopped
    // The write buffer should be empty now.
    // So there should be a call, but nothing should be written.
    BOOST_CHECK_EQUAL(fs_ops_->last_frag_size(), 0);
    BOOST_CHECK(!fs_ops_->agg_write_frag_called());
    BOOST_CHECK(!fs_ops_->agg_write_final_frag_called());
    BOOST_CHECK_EQUAL(fs_ops_->wtrans().valid(), false);
    BOOST_CHECK(!fs_ops_->wtrans().finished());
}

BOOST_AUTO_TEST_SUITE_END()
