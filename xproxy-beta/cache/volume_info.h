#pragma once

#include "unit_blocks.h"

namespace cache
{
namespace detail
{

class volume_info
{
    store_blocks64_t cnt_blocks_;
    bytes32_t hw_sector_size_ = 0;
    bytes32_t alignment_      = 0;
    bytes32_t skip_bytes_     = 0;
    boost::container::string path_;

public:
    explicit volume_info(const boost::container::string& path) noexcept;
    ~volume_info();

    void set_size(bytes64_t bytes) noexcept;
    void set_hw_sector_size(bytes32_t bytes) noexcept;
    void set_alignment(bytes32_t bytes) noexcept;
    void set_skip_bytes(bytes32_t bytes) noexcept;

    const boost::container::string& path() const noexcept { return path_; }
    store_blocks64_t cnt_blocks() const noexcept { return cnt_blocks_; }
    bytes64_t size() const noexcept { return cnt_blocks_.to_bytes(); }
    bytes64_t avail_size() const noexcept { return size() - skip_bytes_; }
    bytes32_t hw_sector_size() const noexcept { return hw_sector_size_; }
    bytes32_t alignment() const noexcept { return alignment_; }
    bytes32_t skip_bytes() const noexcept { return skip_bytes_; }
};

std::ostream& operator<<(std::ostream& os, const volume_info& rhs) noexcept;

////////////////////////////////////////////////////////////////////////////////

/// The function loads the volume_info from the disk/file given by the path.
/// It throws std::system_error or std::logic_error in case of error.
volume_info load_check_volume_info(const boost::container::string& path);

} // namespace detail
} // namespace span
