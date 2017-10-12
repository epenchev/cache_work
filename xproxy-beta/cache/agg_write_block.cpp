#include "precompiled.h"
#include "agg_write_block.h"
#include "cache_stats.h"
#include "object_frag_hdr.h"
#include "range.h"
#include "range_elem.h"
#include "memory_writer.h"

namespace cache
{
namespace detail
{

static_assert((agg_write_meta_size % volume_blocks64_t::block_size) == 0, "");
static_assert((agg_write_block_size % volume_blocks64_t::block_size) == 0, "");

agg_write_block::agg_write_block() noexcept
    : block_meta_(agg_write_meta_size),
      block_data_(alloc_page_aligned(agg_write_block_size)),
      buff_pos_(volume_blocks64_t::create_from_bytes(agg_write_meta_size))
{
}

agg_write_block::~agg_write_block() noexcept
{
}

expected_t<range_elem, agg_write_block::fail_res>
agg_write_block::add_fragment(const fs_node_key_t& key,
                              const range& rng,
                              volume_blocks64_t curr_write_offs,
                              const frag_ro_buff_t& frag) noexcept
{
    X3ME_ASSERT(!pending_disk_write_, "The function is called in wrong state");
    X3ME_ASSERT(frag.size() <= object_frag_max_data_size, "Fragment too big");
    X3ME_ASSERT(rng.len() == frag.size(),
                "The range must correspond to the provided fragment");
    constexpr auto max_size =
        volume_blocks64_t::create_from_bytes(agg_write_block_size);
    const auto fin_size =
        volume_blocks64_t::create_from_bytes(object_frag_size(frag.size()));

    if ((buff_pos_ + fin_size) > max_size)
        return boost::make_unexpected(fail_res::no_space_data);

    const auto disk_offs = curr_write_offs + buff_pos_;
    const range_elem re = make_range_elem(rng.beg(), rng.len(), disk_offs);
    switch (block_meta_.add_entry(key, re))
    {
    case agg_write_meta::add_res::ok:
    {
        const auto hdr  = object_frag_hdr::create(key, re);
        const auto wpos = block_data_.get() + buff_pos_.to_bytes();
        ::memcpy(wpos, &hdr, sizeof(hdr));
        ::memcpy(wpos + sizeof(hdr), frag.data(), frag.size());
        X3ME_ASSERT(fin_size.to_bytes() >= (sizeof(hdr) + frag.size()),
                    "Wrong calculation of the final fragment size");
        buff_pos_ += fin_size;
        return re;
    }
    case agg_write_meta::add_res::overlaps:
        return boost::make_unexpected(fail_res::overlaps);
    case agg_write_meta::add_res::no_space:
        return boost::make_unexpected(fail_res::no_space_meta);
    }
    X3ME_ASSERT(false, "Missing switch case above");
}

bool agg_write_block::try_read_fragment(const fs_node_key_t& key,
                                        const range_elem& rng,
                                        volume_blocks64_t curr_write_offs,
                                        frag_wr_buff_t buff) const noexcept
{
    if (block_meta_.has_entry(key, rng))
    {
        const auto rng_size = rng.rng_size();
        const auto rng_dlen = object_frag_size(rng_size);
        const auto beg_doff = curr_write_offs.to_bytes();
        const auto end_doff = (curr_write_offs + buff_pos_).to_bytes();
        const auto rng_boff = rng.disk_offset().to_bytes();
        const auto rng_eoff = rng.disk_offset().to_bytes() + rng_dlen;
        X3ME_ASSERT(x3me::math::in_range(rng_boff, beg_doff, end_doff) &&
                        (rng_eoff <= end_doff),
                    "The fragment disk range must be fully inside the current "
                    "aggregate range, because it's been found in the metadata");

        const auto buf_off   = rng_boff - beg_doff;
        const auto read_size = object_frag_size(rng_size);
        X3ME_ASSERT(read_size == buff.size(),
                    "The range must correspond to the provided buffer");
        ::memcpy(buff.data(), block_data_.get() + buf_off, read_size);

        return true;
    }
    return false;
}

agg_write_block::agg_ro_buff_t
agg_write_block::begin_disk_write(stats_fs_wr& sts) noexcept
{
    pending_disk_write_ = true;
    // Save the metadata at the beginning of the memory block
    memory_writer w(block_data_.get(), agg_write_meta_size);
    block_meta_.save(w);
    // We write to disk in store block size.
    const auto sz = round_to_store_block_size(buff_pos_.to_bytes());
    X3ME_ASSERT(sz <= agg_write_block_size, "Wrong buff_pos calculations");

    sts.written_meta_size_ = w.buff_size();
    sts.wasted_meta_size_  = w.buff_size() - w.written();
    sts.written_data_size_ = agg_write_block_size;
    sts.wasted_data_size_  = agg_write_block_size - sz;

    return agg_ro_buff_t(block_data_.get(), sz);
}

std::vector<agg_meta_entry> agg_write_block::end_disk_write() noexcept
{
    pending_disk_write_ = false;

    buff_pos_ = volume_blocks64_t::create_from_bytes(agg_write_meta_size);

    // Return the entries and reset the meta with one call
    return block_meta_.release_entries();
}

agg_write_block::agg_wr_buff_t agg_write_block::metadata_buff() noexcept
{
    return agg_wr_buff_t(block_data_.get(), agg_write_meta_size);
}

bytes32_t agg_write_block::bytes_avail() const noexcept
{
    return buff_pos_.to_bytes() - agg_write_meta_size;
}

bytes32_t agg_write_block::free_space() const noexcept
{
    return agg_write_data_size - bytes_avail();
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os,
                         const agg_write_block::fail_res& rhs) noexcept
{
    switch (rhs)
    {
    case agg_write_block::fail_res::overlaps:
        return os << "overlaps";
    case agg_write_block::fail_res::no_space_meta:
        return os << "no_space_meta";
    case agg_write_block::fail_res::no_space_data:
        return os << "no_space_data";
    };
    X3ME_ASSERT(false, "Missing switch case above");
}

} // namespace detail
} // namespace cache
