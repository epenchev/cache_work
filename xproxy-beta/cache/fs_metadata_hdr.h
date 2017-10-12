#pragma once

#include "fs_version.h"

namespace cache
{
namespace detail
{

// The header of the filesystem metadata
class fs_metadata_hdr
{
protected:
    uint64_t magic_;
    time_t create_time_;
    // The UUID is needed because distinguishing the volumes by their paths
    // is not reliable because sometimes they change their letter.
    uuid_t uuid_;
    fs_version version_;
    // The field is used to find out which copy of the metadata (A or B)
    // is fresher. It's allowed to overflow.
    uint32_t sync_serial_;

public:
    static constexpr uint64_t current_magic() noexcept
    {
        return 0x0123F00D3210CAFE;
    }

    static constexpr fs_version current_version() noexcept
    {
        return fs_version::create(0, 3);
    }

    static constexpr auto size() noexcept { return sizeof(fs_metadata_hdr); }

    void clean_init() noexcept // Used instead constructor
    {
        magic_       = current_magic();
        create_time_ = ::time(nullptr);
        // This call is not effective at all, but clean initialization of a FS
        // should be pretty rare event (once in days, weeks, even months) and
        // is done only at cache startup.
        uuid_        = boost::uuids::random_generator()();
        version_     = current_version();
        sync_serial_ = 0;
    }

    auto magic() const noexcept { return magic_; }
    auto create_time() const noexcept { return create_time_; }
    auto version() const noexcept { return version_; }
    const uuid_t& uuid() const noexcept { return uuid_; }

    uint32_t sync_serial() const noexcept { return sync_serial_; }
    void inc_sync_serial() noexcept { ++sync_serial_; }
    void dec_sync_serial() noexcept { --sync_serial_; }

    bool is_current() const noexcept
    {
        // The 'create_time_' is not checked because it can get turned back
        return (magic_ == current_magic()) && (version_ == current_version());
    }
};
static_assert(std::is_pod<fs_metadata_hdr>::value,
              "Needs to be POD because it maps to raw memory");

std::ostream& operator<<(std::ostream& os, const fs_metadata_hdr& rhs) noexcept;

} // namespace detail
} // namespace cache
