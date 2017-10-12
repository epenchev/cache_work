#include "precompiled.h"
#include "fs_metadata.h"
#include "agg_meta_entry.h"
#include "cache_stats.h"
#include "disk_reader.h"
#include "memory_writer.h"
#include "object_key.h"
#include "range.h"
#include "range_vector.h"
#include "volume_info.h"
#include "write_transaction.h"

namespace cache
{
namespace detail
{

static bytes64_t avail_disk_space(const volume_info& vi,
                                  bytes32_t min_avg_obj_size) noexcept
{
    auto metadata_disk_size = [](bytes64_t disk_space, bytes32_t obj_size)
    {
        // We intentionally waste some space here aligning the fs_metadata_ftr
        // at store_block_size and leaving additional space after the footer.
        // We do this because on initialization we need to load only the
        // header, the ops data and the footer in order to find out which
        // of the two copies of the metadata to use.
        // We need the footer alignment and size bigger because we always do
        // the disk operations in store_blocks.
        const bytes64_t ms =
            round_to_store_block_size(
                fs_metadata_hdr::size() + fs_ops_data::size() +
                fs_table::max_full_size(disk_space, obj_size)) +
            round_to_store_block_size(fs_metadata_ftr::size());
        X3ME_ASSERT(disk_space > ms);
        return ms;
    };
    bytes64_t md_size    = 0;
    bytes64_t disk_space = vi.avail_size();
    // We use successive approximation to calculate the available space.
    // We don't have the exact number at the end, but it's close enough.
    md_size = metadata_disk_size(disk_space - md_size, min_avg_obj_size);
    md_size = metadata_disk_size(disk_space - md_size, min_avg_obj_size);
    md_size = metadata_disk_size(disk_space - md_size, min_avg_obj_size);
    return disk_space - md_size;
}

////////////////////////////////////////////////////////////////////////////////

fs_metadata::fs_metadata(const volume_info& vi,
                         bytes32_t min_avg_obj_size) noexcept
    : table_(avail_disk_space(vi, min_avg_obj_size), min_avg_obj_size)
{
    X3ME_ENFORCE(vi.avail_size() > (2 * max_size_on_disk()));
}

fs_metadata::~fs_metadata() noexcept
{
}

fs_metadata::fs_metadata(const fs_metadata& rhs) noexcept
    : hdr_(rhs.hdr_),
      ops_(rhs.ops_),
      table_(rhs.table_),
      ftr_(rhs.ftr_),
      is_dirty_(rhs.is_dirty_)
{
}

fs_metadata::fs_metadata(fs_metadata&& rhs) noexcept
    : hdr_(std::move(rhs.hdr_)),
      ops_(std::move(rhs.ops_)),
      table_(std::move(rhs.table_)),
      ftr_(std::move(rhs.ftr_)),
      is_dirty_(std::exchange(rhs.is_dirty_, false))
{
}

fs_metadata& fs_metadata::operator=(fs_metadata&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        hdr_      = std::move(rhs.hdr_);
        ops_      = std::move(rhs.ops_);
        table_    = std::move(rhs.table_);
        ftr_      = std::move(rhs.ftr_);
        is_dirty_ = std::exchange(rhs.is_dirty_, false);
    }
    return *this;
}

void fs_metadata::clean_init(bytes64_t init_data_offs) noexcept
{

    hdr_.clean_init();
    ops_.clean_init(init_data_offs);
    table_.clean_init();
    ftr_      = hdr_;
    is_dirty_ = false;
}

bool fs_metadata::load(disk_reader& reader)
{
    bool res    = false;
    const int r = load_check_metadata_hdr_ftr(reader);
    if (r >= 0)
    {
        res = load_full_metadata(reader, r);
    }
    return res;
}

void fs_metadata::save(memory_writer& writer) const noexcept
{
    X3ME_ENFORCE(hdr_.is_current() && ftr_.is_current() &&
                 (hdr_.create_time() == ftr_.create_time()));

    writer.write(&hdr_, sizeof(hdr_));
    writer.write(&ops_, sizeof(ops_));
    table_.save(writer);
    // The footer is always written at offset multiple of the store_block_size
    const auto final_size = size_on_disk();
    writer.set_next_offset(final_size - store_block_size);
    writer.write(&ftr_, sizeof(ftr_));
    // Pretend that multiple of the store_block_size has been written
    static_assert(
        sizeof(ftr_) <= store_block_size,
        "We pretend here that the footer consumes full store_block_size");
    writer.set_next_offset(final_size);
}

bytes64_t fs_metadata::size_on_disk() const noexcept
{
    return round_to_store_block_size(hdr_.size() + ops_.size() +
                                     table_.size_on_disk()) +
           round_to_store_block_size(ftr_.size());
}

bytes64_t fs_metadata::max_size_on_disk() const noexcept
{
    return round_to_store_block_size(hdr_.size() + ops_.size() +
                                     table_.max_size_on_disk()) +
           round_to_store_block_size(ftr_.size());
}

////////////////////////////////////////////////////////////////////////////////

bool fs_metadata::rem_table_entry(const fs_node_key_t& key,
                                  const range_elem& rng) noexcept
{
    auto ret = rem_table_entries(key, [&](range_vector& rvec)
                                 {
                                     bytes64_t rem_size = 0;
                                     auto found = rvec.find_exact_range(rng);
                                     if (found != rvec.cend())
                                     {
                                         rem_size = found->rng_size();
                                         rvec.rem_range(found);
                                     }
                                     return rem_size;
                                 });
    return (ret && (ret.value() > 0));
}

////////////////////////////////////////////////////////////////////////////////

void fs_metadata::inc_sync_serial() noexcept
{
    // Modifying sync_serial doesn't make the metadata dirty
    hdr_.inc_sync_serial();
    ftr_ = hdr_;
}

void fs_metadata::dec_sync_serial() noexcept
{
    // Modifying sync_serial doesn't make the metadata dirty
    hdr_.dec_sync_serial();
    ftr_ = hdr_;
}

void fs_metadata::inc_write_pos(bytes64_t pos) noexcept
{
    ops_.inc_write_pos(pos);
    is_dirty_ = true;
}

void fs_metadata::wrap_write_pos(bytes64_t init_write_pos) noexcept
{
    ops_.wrap_write_pos(init_write_pos);
    is_dirty_ = true;
}

#ifdef X3ME_TEST
void fs_metadata::set_write_pos(bytes64_t write_pos,
                                uint64_t write_lap) noexcept
{
    ops_.set_write_pos(write_pos);
    ops_.set_write_lap(write_lap);
}
#endif // X3ME_TEST

void fs_metadata::get_stats(stats_fs_md& smd, stats_fs_ops& sops) const noexcept
{
    smd.cnt_entries_           = table_.cnt_entries();
    smd.cnt_nodes_             = table_.cnt_fs_nodes();
    smd.cnt_ranges_            = table_.cnt_ranges();
    smd.max_allowed_data_size_ = table_.max_allowed_data_size();
    smd.entries_data_size_     = table_.entries_data_size();

    smd.curr_data_size_ = fs_table::data_size(smd.cnt_nodes_, smd.cnt_ranges_);

    sops.write_pos_ = ops_.write_pos();
    sops.write_lap_ = ops_.write_lap();
}

////////////////////////////////////////////////////////////////////////////////

int fs_metadata::load_check_metadata_hdr_ftr(disk_reader& reader)
{
    const auto offs_hdr_a = 0;
    const auto offs_hdr_b = max_size_on_disk();

    auto get_ftr_offs = [](disk_reader& reader)
    {
        fs_ops_data unused;
        reader.read(&unused, sizeof(unused)); // Skip it
        fs_table::disk_hdr hdr;
        fs_table::err_info_t err_info;
        if (!fs_table::load(reader, hdr, err_info))
        {
            XLOG_ERROR(disk_tag, "Corrupted cache FS table for volume '{}'. {}",
                       reader.path(),
                       string_view_t(err_info.data(), err_info.size()));
            return bytes64_t{0};
        }
        return round_to_store_block_size(
            fs_metadata_hdr::size() + fs_ops_data::size() +
            fs_table::full_size(hdr.table_data_size_));
    };

    // We need to read A and B headers and footers
    fs_metadata_hdr hdr_a, hdr_b;
    fs_metadata_ftr ftr_a, ftr_b;

    reader.set_next_offset(offs_hdr_a);
    reader.read(&hdr_a, sizeof(hdr_a));
    auto offs = get_ftr_offs(reader);
    if (offs == 0)
        return -1;
    reader.set_next_offset(offs_hdr_a + offs);
    reader.read(&ftr_a, sizeof(ftr_a));

    reader.set_next_offset(offs_hdr_b);
    reader.read(&hdr_b, sizeof(hdr_b));
    offs = get_ftr_offs(reader);
    if (offs == 0)
        return -1;
    reader.set_next_offset(offs_hdr_b + offs);
    reader.read(&ftr_b, sizeof(ftr_b));

    // Check them all :)
    if (!hdr_a.is_current() || !ftr_a.is_current() || !hdr_b.is_current() ||
        !ftr_b.is_current() || (hdr_a.uuid() != ftr_a.uuid()) ||
        (hdr_b.uuid() != ftr_b.uuid()))
    {
        XLOG_WARN(disk_tag,
                  "The cache FS metadata on volume '{}' is invalid or not "
                  "current. Current version {}.\n"
                  "HdrA {}\nFtrA {}\nHdrB {}\nFtrB {}",
                  reader.path(), fs_metadata_hdr::current_version(), hdr_a,
                  ftr_a, hdr_b, ftr_b);
        return -1;
    }

    int res = -1;
    // Last choose the most current metadata to load
    if ((hdr_a.sync_serial() == ftr_a.sync_serial()) &&
        ((hdr_a.sync_serial() >= hdr_b.sync_serial()) ||
         (hdr_b.sync_serial() != ftr_b.sync_serial())))
    {
        res = 0; // Use the A metadata
        XLOG_DEBUG(disk_tag,
                   "The A copy of FS metadata on volume '{}' is fresher.\n"
                   "HdrA {}\nFtrA {}\nHdrB {}\nFtrB {}",
                   reader.path(), hdr_a, ftr_a, hdr_b, ftr_b);
    }
    else if (hdr_b.sync_serial() == ftr_b.sync_serial())
    {
        res = 1; // Use the B metadata
        XLOG_DEBUG(disk_tag,
                   "The B copy of FS metadata on volume '{}' is fresher.\n"
                   "HdrA {}\nFtrA {}\nHdrB {}\nFtrB {}",
                   reader.path(), hdr_a, ftr_a, hdr_b, ftr_b);
    }
    else
    {
        XLOG_WARN(
            disk_tag,
            "The cache FS metadata on volume '{}' has messed sync_serials.\n"
            "HdrA {}\nFtrA {}\nHdrB {}\nFtrB {}",
            reader.path(), hdr_a, ftr_a, hdr_b, ftr_b);
    }

    return res;
}

bool fs_metadata::load_full_metadata(disk_reader& reader, uint32_t metadata_idx)
{
    assert((metadata_idx == 0) || (metadata_idx == 1));
    constexpr const char lbl[] = "AB";
    const auto md_offs         = metadata_idx * max_size_on_disk();

    XLOG_DEBUG(
        disk_tag,
        "Loading full cache FS {} metadata for volume '{}' from rel_offset {}",
        lbl[metadata_idx], reader.path(), md_offs);

    fs_metadata_hdr hdr;
    fs_ops_data ops;
    fs_table tbl(table_); // The copy here is cheap because the table is empty
    fs_metadata_ftr ftr;

    reader.set_next_offset(md_offs);
    reader.read(&hdr, sizeof(hdr));
    reader.read(&ops, sizeof(ops));
    fs_table::err_info_t err_info;
    if (!tbl.load(reader, err_info))
    {
        XLOG_ERROR(disk_tag, "Corrupted cache FS table for volume '{}'. {}",
                   reader.path(),
                   string_view_t(err_info.data(), err_info.size()));
        return false;
    }
    // The footer is always written at offset multiple of the store_block_size
    const auto ftr_offs =
        round_to_store_block_size(hdr.size() + ops.size() + tbl.size_on_disk());
    reader.set_next_offset(md_offs + ftr_offs);
    reader.read(&ftr, sizeof(ftr));

    // Check again the loaded metadata
    if (!hdr.is_current() || !ftr.is_current() ||
        (hdr.create_time() != ftr.create_time()))
    {
        XLOG_ERROR(disk_tag,
                   "The final loaded cache FS metadata for volume '{}' is "
                   "invalid or not current. "
                   "Disk offset {} bytes and size {} bytes.\nHdr {}\nFtr {}",
                   reader.path(), md_offs, size_on_disk(), hdr, ftr);
        return false;
    }

    // Everything seems correct. We can populate the member values.
    hdr_   = hdr;
    ops_   = ops;
    table_ = std::move(tbl);
    ftr_   = ftr;

    return true;
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const fs_metadata& rhs) noexcept
{
    // clang-format off
    return os << "{hdr: " << rhs.hdr_
              << ",\ndata_ops: " << rhs.ops_
              << ",\nnode_table: " << rhs.table_
              << ",\nftr: " << rhs.ftr_ << '}';
    // clang-format on
}

} // namespace detail
} // namespace cache
