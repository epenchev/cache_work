#pragma once

#include "fs_metadata_hdr.h"

namespace cache
{
namespace detail
{

// The header of the filesystem footer is exactly the same as the header.
// It's put to ensure that the data after the FS metadata is not corrupted
// by writing beyond the metadata end.
struct fs_metadata_ftr : fs_metadata_hdr
{
    using fs_metadata_hdr::operator=;
};
static_assert(std::is_pod<fs_metadata_ftr>::value,
              "Needs to be POD because it maps to raw memory");

} // namespace detail
} // namespace cache
