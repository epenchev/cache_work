#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "cache_fs_ops_empty.h"
#include "../../cache/task_md_sync.h"

using namespace cache::detail;

namespace
{
// A helper class to test the object_write_handle functionality
class fs_ops_impl final : public cache_fs_ops_empty
{
    io_service_t ios_;
    io_service_t::work iow_{ios_};

    std::vector<uint8_t> all_data_;

public:
    bytes64_t last_write_offs_ = 0;
    bytes64_t last_write_size_ = 0;

public:
    fs_ops_impl() noexcept {}
    ~fs_ops_impl() noexcept final {}

    void run_one() noexcept { ios_.poll_one(); }

    const std::vector<uint8_t>& all_data() const noexcept { return all_data_; }

    void aios_push_write_queue(owner_ptr_t<aio_task> t) noexcept final
    {
        ios_.post([this, t]
                  {
                      auto d = t->on_begin_io_op();
                      BOOST_REQUIRE_MESSAGE(t->operation() == aio_op::write,
                                            "Must do write operations only");
                      BOOST_REQUIRE_MESSAGE(d, "Must not skip operations");
                      last_write_offs_ = d->offs_;
                      last_write_size_ = d->size_;
                      const auto sz = all_data_.size();
                      all_data_.resize(sz + d->size_);
                      ::memcpy(&all_data_[sz], d->buf_, d->size_);
                      err_code_t err; // Won't test cache errors
                      t->on_end_io_op(err);
                  });
    }
};
} // namespace
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE(task_md_sync_tests)

BOOST_AUTO_TEST_CASE(metadata_sync)
{
    constexpr auto cnt                = 3;
    constexpr bytes64_t all_data_size = cnt * metadata_sync_size + 128_KB;
    constexpr bytes64_t data_size     = metadata_sync_size;
    bytes64_t disk_offs               = 20_MB;

    bool on_end_called = false;
    auto on_end        = [&](bool r)
    {
        BOOST_REQUIRE_MESSAGE(r, "Must be successful");
        on_end_called = true;
    };

    std::vector<uint8_t> data(all_data_size, 'e');
    for (auto i = 0; i < cnt; ++i)
    {
        ::memset(&data[i * data_size], 'a' + i, data_size);
    }

    fs_ops_impl fs_ops;
    auto pdata = alloc_page_aligned(all_data_size);
    ::memcpy(pdata.get(), data.data(), all_data_size);
    auto task = make_aio_task<task_md_sync>(&fs_ops, std::move(pdata),
                                            all_data_size, disk_offs, on_end);
    fs_ops.aios_push_write_queue(task.get());

    for (auto i = 0; i < cnt; ++i)
    {
        fs_ops.run_one();
        BOOST_CHECK_EQUAL(fs_ops.last_write_offs_, disk_offs + (i * data_size));
        BOOST_CHECK_EQUAL(fs_ops.last_write_size_, data_size);
    }
    // Last write
    fs_ops.run_one();
    BOOST_CHECK_EQUAL(fs_ops.last_write_offs_, disk_offs + (cnt * data_size));
    BOOST_CHECK_EQUAL(fs_ops.last_write_size_,
                      all_data_size - (cnt * data_size));

    BOOST_REQUIRE_EQUAL(fs_ops.all_data().size(), data.size());
    BOOST_CHECK(fs_ops.all_data() == data);
}

BOOST_AUTO_TEST_SUITE_END()
