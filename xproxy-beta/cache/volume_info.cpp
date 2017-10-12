#include "precompiled.h"
#include "volume_info.h"
#include "volume_fd.h"

namespace cache
{
namespace detail
{

volume_info::volume_info(const boost::container::string& path) noexcept
    : path_(path)
{
}

volume_info::~volume_info()
{
}

void volume_info::set_size(bytes64_t bytes) noexcept
{
    // The size may not be multiple of the store block size and thus
    // we need to round it down.
    cnt_blocks_ = store_blocks64_t::round_down_to_blocks(bytes);
}

void volume_info::set_hw_sector_size(bytes32_t bytes) noexcept
{
    hw_sector_size_ = bytes;
}

void volume_info::set_alignment(bytes32_t bytes) noexcept
{
    alignment_ = bytes;
}

void volume_info::set_skip_bytes(bytes32_t bytes) noexcept
{
    skip_bytes_ = bytes;
}

std::ostream& operator<<(std::ostream& os, const volume_info& rhs) noexcept
{
    // clang-format off
    os << "{path: " << rhs.path()
       << ", size_bytes: " << rhs.size() 
       << ", size_blocks: " << rhs.cnt_blocks()
       << ", hw_sector_size: " << rhs.hw_sector_size()
       << ", alignment: " << rhs.alignment()
       << ", skip_bytes: " << rhs.skip_bytes()
       << '}';
    // clang-format on
    return os;
}

////////////////////////////////////////////////////////////////////////////////

volume_info load_check_volume_info(const boost::container::string& path)
{
    auto throw_sys_err = [](const char* descr)
    {
        throw bsys::system_error(errno, bsys::get_system_category(), descr);
    };

    volume_info res(path);

    err_code_t unused;
    volume_fd fd;
    if (!fd.open(path.c_str(), unused))
        throw_sys_err("OS call 'open' failed");

    struct stat stats;
    if (::fstat(fd.get(), &stats) == -1)
        throw_sys_err("OS call 'fstat' failed");

    switch (stats.st_mode & S_IFMT)
    {
    case S_IFBLK:
    case S_IFCHR:
    { // Block device
        if (major((stats.st_rdev) == RAW_MAJOR) && (minor(stats.st_rdev) == 0))
            throw_sys_err("Unsupported raw device");

        uint64_t ui64;
        uint32_t ui32;

        // Get the block device size in bytes.
        if (::ioctl(fd.get(), BLKGETSIZE64, &ui64) == 0)
            res.set_size(ui64);
        else
            throw_sys_err("OS call 'ioctl BLKGETSIZE64' faield");

        // Get the logical block size in bytes.
        if (::ioctl(fd.get(), BLKSSZGET, &ui32) == 0)
            res.set_hw_sector_size(ui32);
        else
            throw_sys_err("OS call 'ioctl BLKSZGET' faield");

        // GET the number of bytes needed to align the I/Os to the block device.
        // This might be non-zero for logical volume backed by JBOD or RAID
        // device(s).
        if (::ioctl(fd.get(), BLKALIGNOFF, &ui32) == 0)
            res.set_alignment(ui32);
        else
            throw_sys_err("OS call 'ioctl BLKALIGNOFF' faield");
    }
    break;
    case S_IFREG:
    { // Regular file.
        struct statvfs stats_vfs;
        if (::fstatvfs(fd.get(), &stats_vfs) == -1)
            throw_sys_err("OS call 'fstatvfs' failed");

        res.set_size(stats.st_size);
        res.set_hw_sector_size(stats_vfs.f_bsize);
        res.set_alignment(0);
    }
    break;
    default:
        throw std::logic_error(
            "Unsupported device type. Only block devices and files are "
            "supported");
    }

    if (res.size() < min_volume_size)
    {
        x3me::utils::string_builder_128 err;
        err << "Volume too small. Size " << res.size()
            << " bytes. Min volume size " << min_volume_size << " bytes\0";
        throw std::logic_error(err.data());
    }

    if (res.hw_sector_size() != volume_block_size)
    {
        x3me::utils::string_builder_128 err;
        err << "Unsupported HW sector size " << res.hw_sector_size()
            << " bytes. The supported size is " << volume_block_size
            << " bytes. The issue needs to be fixed in the xproxy code\0";
        throw std::logic_error(err.data());
    }

    if ((res.alignment() != 0) && ((volume_skip_bytes % res.alignment()) != 0))
    {
        x3me::utils::string_builder_128 err;
        err << "Strange and unsupported volume alignment_offset "
            << res.alignment() << " bytes\0";
        throw std::logic_error(err.data());
    }
    res.set_skip_bytes(volume_skip_bytes);
    X3ME_ENFORCE(res.skip_bytes() < res.size());

    return res;
}

} // namespace detail
} // namespace cache
