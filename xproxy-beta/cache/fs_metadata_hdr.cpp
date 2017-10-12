#include "precompiled.h"
#include "fs_metadata_hdr.h"

namespace cache
{
namespace detail
{

std::ostream& operator<<(std::ostream& os, const fs_metadata_hdr& rhs) noexcept
{
    os << "{magic: " << rhs.magic() << "; create_time: ";

    const auto ct = rhs.create_time();
    tm ctm;
    ::localtime_r(&ct, &ctm);

    try
    {
        char buf[32];
        fmt::ArrayWriter aw(buf);
        aw.write("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                 1900 + ctm.tm_year, ctm.tm_mon + 1, ctm.tm_mday, ctm.tm_hour,
                 ctm.tm_min, ctm.tm_sec);
        os.write(aw.data(), aw.size());
    }
    catch (...)
    {
        // Can't format the time. Print the unix timestamp directly.
        os << ct;
    }

    os << "; uuid: " << rhs.uuid();
    os << "; version: " << rhs.version();
    os << "; sync_serial: " << rhs.sync_serial() << '}';

    return os;
}

} // namespace detail
} // namespace cache
