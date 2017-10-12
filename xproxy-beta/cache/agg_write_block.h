#pragma once

#include "agg_write_meta.h"
#include "aligned_data_ptr.h"
#include "unit_blocks.h"

namespace cache
{
struct stats_fs_wr;
namespace detail
{

class agg_meta_entry;
class object_frag_hdr;
class range;
class range_elem;
class write_buffer;

class agg_write_block
{
    // TODO Possible optimization here is to keep the agg_write_meta
    // to work in-place at the beginning of the agg_write_data buffer.
    // This way we won't waste additional memory and we won't need
    // even memcpy for the serialization part.
    agg_write_meta block_meta_;
    aligned_data_ptr_t block_data_;
    volume_blocks64_t buff_pos_;
    // The flag is needed only to ensure the correct usage of the class.
    // Will probably be removed in the future.
    bool pending_disk_write_ = false;

public:
    agg_write_block() noexcept;
    ~agg_write_block() noexcept;

    agg_write_block(const agg_write_block&) = delete;
    agg_write_block& operator=(const agg_write_block&) = delete;
    agg_write_block(agg_write_block&&) = delete;
    agg_write_block& operator=(agg_write_block&&) = delete;

    using frag_ro_buff_t = x3me::mem_utils::array_view<const uint8_t>;
    using frag_wr_buff_t = x3me::mem_utils::array_view<uint8_t>;
    enum struct fail_res
    {
        overlaps,
        no_space_meta,
        no_space_data,
    };
    expected_t<range_elem, fail_res>
    add_fragment(const fs_node_key_t& key,
                 const range& rng,
                 volume_blocks64_t curr_write_offs,
                 const frag_ro_buff_t& frag) noexcept;
    bool try_read_fragment(const fs_node_key_t& key,
                           const range_elem& rng,
                           volume_blocks64_t curr_write_offs,
                           frag_wr_buff_t buff) const noexcept;

    // Returns read-only (RO) buffer
    using agg_ro_buff_t = x3me::mem_utils::array_view<const uint8_t>;
    agg_ro_buff_t begin_disk_write(stats_fs_wr& sts) noexcept;
    std::vector<agg_meta_entry> end_disk_write() noexcept;

    using agg_wr_buff_t = x3me::mem_utils::array_view<uint8_t>;
    // Unsafe method. Provides buffer for temporary usage.
    // It's unsafe the buffer to be used when there is a pending disk write
    // i.e. between the calls of begin_disk_write/end_disk_write.
    agg_wr_buff_t metadata_buff() noexcept;

    bytes32_t bytes_avail() const noexcept;
    bytes32_t free_space() const noexcept;

    static constexpr volume_blocks64_t max_size() noexcept
    {
        return volume_blocks64_t::create_from_bytes(agg_write_block_size);
    }
};

std::ostream& operator<<(std::ostream& os,
                         const agg_write_block::fail_res& rhs) noexcept;

} // namespace detail
} // namespace cache
