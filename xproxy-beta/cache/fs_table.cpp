#include "precompiled.h"
#include "fs_table.h"
#include "disk_reader.h"
#include "memory_writer.h"

namespace cache
{
namespace detail
{

size_t fs_table::fs_node_hash::operator()(const fs_node_key_t& v) const noexcept
{
    return boost::hash_range(v.begin(), v.end());
}

constexpr uint64_t fs_table::disk_hdr::magic;

////////////////////////////////////////////////////////////////////////////////

fs_table::fs_table(bytes64_t avail_disk_space,
                   bytes32_t min_avg_obj_size) noexcept
    : max_allowed_data_size_(max_data_size(avail_disk_space, min_avg_obj_size))
{
    fs_nodes_.set_deleted_key(fs_node_key_t::zero());
}

fs_table::~fs_table() noexcept
{
}

fs_table::fs_table(const fs_table& rhs) noexcept
    : max_allowed_data_size_(rhs.max_allowed_data_size_),
      cnt_ranges_(rhs.cnt_ranges_),
      cnt_entries_(rhs.cnt_entries_),
      entries_data_size_(rhs.entries_data_size_),
      fs_nodes_(rhs.fs_nodes_)
{
}

fs_table::fs_table(fs_table&& rhs) noexcept
    : max_allowed_data_size_(rhs.max_allowed_data_size_),
      cnt_ranges_(std::exchange(rhs.cnt_ranges_, 0)),
      cnt_entries_(std::exchange(rhs.cnt_entries_, 0)),
      entries_data_size_(std::exchange(rhs.entries_data_size_, 0))
{
    fs_nodes_t tmp;
    tmp.set_deleted_key(fs_node_key_t::zero());
    tmp.swap(rhs.fs_nodes_);
    fs_nodes_.swap(tmp);
}

fs_table& fs_table::operator=(fs_table&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        const_cast<bytes64_t&>(max_allowed_data_size_) =
            rhs.max_allowed_data_size_;
        cnt_ranges_        = std::exchange(rhs.cnt_ranges_, 0);
        cnt_entries_       = std::exchange(rhs.cnt_entries_, 0);
        entries_data_size_ = std::exchange(rhs.entries_data_size_, 0);

        fs_nodes_t tmp;
        tmp.set_deleted_key(fs_node_key_t::zero());
        tmp.swap(rhs.fs_nodes_);
        fs_nodes_.swap(tmp);
    }
    return *this;
}

void fs_table::clean_init() noexcept
{
    cnt_ranges_ = 0;

    fs_nodes_t tmp;
    tmp.set_deleted_key(fs_node_key_t::zero());
    fs_nodes_.swap(tmp);
}

bool fs_table::load(disk_reader& reader, err_info_t& out_err)
{
    disk_hdr hdr;
    if (!load(reader, hdr, out_err))
        return false;

    if (hdr.table_data_size_ > max_allowed_data_size_)
    {
        out_err << "Invalid value for the fs_nodes count (" << hdr.cnt_nodes_
                << ") and/or the ranges count (" << hdr.cnt_ranges_
                << "). Needed memory bytes " << hdr.table_data_size_
                << ". Max allowed memory bytes " << max_allowed_data_size_;
        return false;
    }

    decltype(cnt_ranges_) num_ranges = 0;
    fs_nodes_t tmp(hdr.cnt_nodes_);
    tmp.set_deleted_key(fs_node_key_t::zero());
    for (decltype(hdr.cnt_nodes_) i = 0; i < hdr.cnt_nodes_; ++i)
    {
        fs_node_t fs_node;
        // Ugly, but needed, and safe. The entry is still not in the map
        auto& hash = const_cast<fs_node_key_t&>(fs_node.first);
        reader.read(hash.buff_unsafe(), hash.size());
        auto res = tmp.insert(fs_node);
        if (!res.second)
        {
            out_err << "Found two times entry with tag " << fs_node.first;
            return false;
        }
        auto& rvec = res.first->second;
        if (!rvec.load(reader))
        {
            out_err << "Invalid range_vector for entry with tag "
                    << fs_node.first;
            return false;
        }
        const auto cnt_before = rvec.size();
        if (cnt_before > 1)
            num_ranges += cnt_before; // Don't count in-place range_elements
        // Unfortunately we need to reset the meta here, because we
        // could have saved the metadata with some temporary bits/bytes set.
        // We trade some startup time for smaller memory consumption on runtime
        // using some bytes from the range_elem metadata for temporary data.
        for (auto it = rvec.begin(); it != rvec.end();)
        {
            if (!it->in_memory())
            {
                rv_elem_reset_meta(it);
                ++it;
            }
            else // This entry hasn't been committed to the disk. Remove it
                it = rvec.rem_range(it);
        }
        const auto cnt_now = rvec.size();
        // Correct the num_ranges with the removed ranges count
        num_ranges -= calc_dec_cnt_ranges(cnt_before, cnt_before - cnt_now);
        if (cnt_now == 0)
            tmp.erase(res.first); // We don't keep empty entries
    }
    if (hdr.cnt_ranges_ != num_ranges)
    {
        out_err << "Invalid value for the ranges count: " << hdr.cnt_ranges_
                << ". Loaded ranges: " << num_ranges;
        return false;
    }
    // Read the footer magic
    hdr.magic_ = 0;
    reader.read(&hdr.magic_, sizeof(hdr.magic_));
    if (hdr.magic_ != disk_hdr::magic)
    {
        out_err << "Invalid fs_table second magic value. Read " << std::hex
                << hdr.magic_ << ". Expected " << disk_hdr::magic;
        return false;
    }

    // Everything seems correct. We can populate the member values.
    cnt_ranges_        = hdr.cnt_ranges_;
    cnt_entries_       = hdr.cnt_entries_;
    entries_data_size_ = hdr.entries_data_size_;
    tmp.swap(fs_nodes_);

    return true;
}

void fs_table::save(memory_writer& writer) const noexcept
{
    disk_hdr hdr           = {};
    hdr.magic_             = disk_hdr::magic;
    hdr.cnt_nodes_         = fs_nodes_.size();
    hdr.cnt_ranges_        = cnt_ranges_;
    hdr.cnt_entries_       = cnt_entries_;
    hdr.table_data_size_   = data_size(hdr.cnt_nodes_, hdr.cnt_ranges_);
    hdr.entries_data_size_ = entries_data_size_;

    writer.write(&hdr, sizeof(hdr));

    const auto pos1 = writer.written();
    for (const auto& i : fs_nodes_)
    {
        writer.write(i.first.data(), i.first.size());
        i.second.save(writer);
    }
    const auto pos2 = writer.written();
    X3ME_ENFORCE((pos2 - pos1) == hdr.table_data_size_,
                 "Wrong calculation of the data size");

    // Write the footer magic
    writer.write(&hdr.magic_, sizeof(hdr.magic_));
}

////////////////////////////////////////////////////////////////////////////////

bytes64_t fs_table::size_on_disk() const noexcept
{
    return full_size(data_size(fs_nodes_.size(), cnt_ranges_));
}

bytes64_t fs_table::max_size_on_disk() const noexcept
{
    return full_size(max_allowed_data_size_);
}

bool fs_table::limit_reached() const noexcept
{
    return !(data_size(fs_nodes_.size(), cnt_ranges_) < max_allowed_data_size_);
}

bool fs_table::load(disk_reader& reader, disk_hdr& hdr, err_info_t& out_err)
{
    static_assert(std::is_pod<disk_hdr>::value, "We do memcpy to it");
    reader.read(&hdr, sizeof(hdr));
    if (hdr.magic_ != disk_hdr::magic)
    {
        out_err << "Invalid fs_table first magic value. Read " << std::hex
                << hdr.magic_ << ". Expected " << disk_hdr::magic;
        return false;
    }
    const auto exp_size = data_size(hdr.cnt_nodes_, hdr.cnt_ranges_);
    if (hdr.table_data_size_ != exp_size)
    {
        out_err << "Invalid value for the fs_nodes count (" << hdr.cnt_nodes_
                << ") and/or the ranges count (" << hdr.cnt_ranges_
                << ") and/or data size (" << hdr.table_data_size_
                << "). Exp data size " << exp_size << " bytes";
        return false;
    }
    return true;
}

bytes64_t fs_table::data_size(uint64_t cnt_fs_nodes,
                              uint64_t cnt_ranges) noexcept
{
    return (cnt_fs_nodes * bytes64_t(sizeof(fs_node_t))) +
           (cnt_ranges * bytes64_t(sizeof(range_elem)));
}

bytes64_t fs_table::full_size(bytes64_t data_size) noexcept
{
    // We have the disk header at the beginning, the actual
    // data and a magic value at the end, to detect overwrite.
    return sizeof(disk_hdr) + data_size + sizeof(disk_hdr::magic_);
}

bytes64_t fs_table::max_full_size(bytes64_t disk_space,
                                  bytes32_t min_object_size) noexcept
{
    return full_size(max_data_size(disk_space, min_object_size));
}

////////////////////////////////////////////////////////////////////////////////

void fs_table::on_inc_entries(const range_elem& rng) noexcept
{
    cnt_entries_ += 1;
    entries_data_size_ += rng.rng_size();
}

void fs_table::on_dec_entries(uint64_t cnt_removed, bytes64_t rem_size) noexcept
{
    cnt_entries_ -= cnt_removed;
    entries_data_size_ -= rem_size;
}

void fs_table::on_dec_entries(const range_elem& rng) noexcept
{
    on_dec_entries(1, rng.rng_size());
}

void fs_table::on_dec_entries(const range_vector::iter_range& rngs) noexcept
{
    const auto size = std::accumulate(rngs.begin(), rngs.end(), bytes64_t{0},
                                      [](bytes64_t sum, const range_elem& rng)
                                      {
                                          return sum + rng.rng_size();
                                      });
    on_dec_entries(rngs.size(), size);
}

////////////////////////////////////////////////////////////////////////////////

bytes64_t fs_table::max_data_size(bytes64_t disk_space,
                                  bytes32_t min_object_size) noexcept
{
    X3ME_ASSERT(disk_space > min_object_size, "Must be much much bigger");
    static_assert(sizeof(fs_node_t) >= sizeof(range_elem), "");
    const auto cnt_objs = disk_space / min_object_size;
    // The worst possible case is when we have single range per fs_node.
    // Thus we use it for the calculations here.
    return cnt_objs * sizeof(fs_node_t);
}

////////////////////////////////////////////////////////////////////////////////

uint32_t fs_table::calc_inc_cnt_ranges(uint32_t rv_size) noexcept
{
    switch (rv_size)
    {
    // The SBO will kick-in and we don't want to count the range in this case
    case 0:
        return 0;
    // The SBO has been active, so a range hasn't been counted so far.
    // Now the existing and the new range should be counted.
    case 1:
        return 2;
    }
    return 1; // No SBO active i.e. just count the new added range
}

uint32_t fs_table::calc_dec_cnt_ranges(uint32_t rv_size, uint32_t dec) noexcept
{
    if (rv_size == 1)
    {
        // The range_elem hasn't been counted so far because of the SBO.
        // We shouldn't count it now.
        return 0;
    }
    else if (rv_size == (dec + 1))
    {
        // The remaining element shouldn't be counted anymore because the
        // SBO of the range_vector will kick-in.
        // Note that we can't enter here if the dec is 0, because in this
        // case the rv_size must be 1, but this case is handled above.
        return dec + 1;
    }
    // We either remove all rv_size or the remaining range_elements are > 1
    // but we don't remove all of them, or don't remove any of them (dec = 0).
    return dec;
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const fs_table& rhs) noexcept
{
    const auto size_fnos = rhs.fs_nodes_.size() * sizeof(fs_table::fs_node_t);
    const auto size_rngs = rhs.cnt_ranges_ * sizeof(range_elem);
    // clang-format off
    return os << "{max_allowed_bytes: " << rhs.max_allowed_data_size_
              << ", bytes_fs_nodes: " << size_fnos
              << ", bytes_ranges: " << size_rngs
              << ", all_bytes: " << size_fnos + size_rngs << '}';
    // clang-format om
}

std::ostream& operator<<(std::ostream& os,
                         const fs_table::add_res& rhs) noexcept
{
    switch (rhs)
    {
        case fs_table::add_res::added:
             os << "Added";
             break;
        case fs_table::add_res::overwrote:
             os << "Overwrote";
             break;
        case fs_table::add_res::skipped:
             os << "Skipped";
             break;
        case fs_table::add_res::limit_reached:
            os << "Memory limits reached";
            break;
        default:
            X3ME_ASSERT(false, "Missing case in the switch");
            break;
    }
    return os;
}

} // namespace detail
} // namespace cache
