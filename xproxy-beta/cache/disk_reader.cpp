#include "precompiled.h"
#include "disk_reader.h"

namespace cache
{
namespace detail
{

disk_reader::disk_reader(const boost::container::string& vol_path,
                         bytes64_t beg_offs,
                         bytes64_t end_offs)
    : buff_(alloc_page_aligned(buff_capacity))
    , beg_disk_offs_(beg_offs)
    , end_disk_offs_(end_offs)
    , vol_path_(vol_path)
{
    fd_.reset(::fopen(vol_path.c_str(), "r"));
    if (!fd_)
    {
        throw bsys::system_error(
            errno, bsys::get_system_category(),
            ("Disk_reader failed to open volume: " + vol_path).c_str());
    }
    // We want the reader to use bigger buffer, our buffer :)
    auto buff = reinterpret_cast<char*>(buff_.get());
    if (::setvbuf(fd_.get(), buff, _IOFBF, buff_capacity) != 0)
    {
        throw bsys::system_error(errno, bsys::get_system_category(),
                                 "Disk_reader unable to set buffer");
    }
    set_next_offset(0);
}

disk_reader::~disk_reader() noexcept
{
}

void disk_reader::set_next_offset(bytes64_t offs)
{
    X3ME_ENFORCE(offs <= read_area_size(), "Invalid offset provided");
    const auto disk_offs = beg_disk_offs_ + offs;
    if (::fseek(fd_.get(), disk_offs, SEEK_SET) != 0)
    {
        x3me::utilities::string_builder_256 inf;
        inf << "Disk_reader. Unable to set next offset to " << disk_offs
            << ". Begin disk offset " << beg_disk_offs_ << '\0';
        throw bsys::system_error(errno, bsys::get_system_category(),
                                 inf.data());
    }
}

void disk_reader::read(void* buf, size_t len)
{
    const auto res = ::fread(buf, 1, len, fd_.get());
    if (res != len)
    {
        x3me::utilities::string_builder_256 inf;
        inf << "Disk_reader read error. Read bytes: " << res
            << ". Wanted bytes: " << len;
        throw bsys::system_error(errno, bsys::get_system_category(),
                                 inf.data());
    }
}

bytes64_t disk_reader::curr_disk_offset() noexcept
{
    const auto ret = ::ftell(fd_.get());
    X3ME_ENFORCE(ret >= 0, "The file handle must be valid");
    return ret;
}

} // namespace detail
} // namespace cache
