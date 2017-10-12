#pragma once

#include "aligned_data_ptr.h"

namespace cache
{
namespace detail
{

// Provides functionality for buffered reading from provided disk area
// Do not use buffered reading along with unbuffered reading/writing.
class disk_reader
{
    static constexpr bytes32_t buff_capacity = 1_MB;

    struct fcloser
    {
        void operator()(FILE* f) noexcept { ::fclose(f); }
    };
    using file_t = std::unique_ptr<FILE, fcloser>;

    file_t fd_;
    aligned_data_ptr_t buff_;
    const bytes64_t beg_disk_offs_;
    const bytes64_t end_disk_offs_; // The max allowed offset on the disk
    const boost::container::string vol_path_;

public:
    disk_reader(const boost::container::string& vol_path,
                bytes64_t beg_offs,
                bytes64_t end_offs);
    ~disk_reader() noexcept;

    disk_reader(const disk_reader&) = delete;
    disk_reader& operator=(const disk_reader&) = delete;
    disk_reader(disk_reader&&) = delete;
    disk_reader& operator=(disk_reader&&) = delete;

    // Must be in [0, (end_offset_ - beg_offset_)).
    void set_next_offset(bytes64_t offs);

    void read(void* buf, size_t len);

    bytes64_t curr_disk_offset() noexcept;
    bytes64_t beg_disk_offset() const noexcept { return beg_disk_offs_; }
    bytes64_t end_disk_offset() const noexcept { return end_disk_offs_; }
    const boost::container::string& path() const noexcept { return vol_path_; }

private:
    auto read_area_size() const noexcept
    {
        return end_disk_offs_ - beg_disk_offs_;
    }
};

} // namespace detail
} // namespace cache
