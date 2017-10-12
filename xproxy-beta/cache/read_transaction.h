#pragma once

#include "object_key.h"

namespace cache
{
namespace detail
{

class read_transaction
{
    static constexpr auto invalid_value = bytes64_t(-1);

    object_key obj_key_;
    bytes64_t read_bytes_ = invalid_value;

public:
#ifdef X3ME_TEST
    read_transaction() noexcept {}
#endif
    explicit read_transaction(const object_key& obj_key) noexcept;
    ~read_transaction() noexcept;

    read_transaction(const read_transaction&) = delete;
    read_transaction& operator=(const read_transaction&) = delete;

    read_transaction(read_transaction&& rhs) noexcept;
    read_transaction& operator=(read_transaction&& rhs) noexcept;

    void inc_read_bytes(bytes64_t bytes) noexcept;

    void invalidate() noexcept;

    const object_key& obj_key() const noexcept { return obj_key_; }
    const fs_node_key_t& fs_node_key() const noexcept
    {
        return obj_key_.fs_node_key();
    }
    const range& get_range() const noexcept { return obj_key_.get_range(); }
    bytes64_t read_bytes() const noexcept { return read_bytes_; }
    bytes64_t curr_offset() const noexcept
    {
        return obj_key_.get_range().beg() + read_bytes_;
    }
    bytes64_t end_offset() const noexcept { return obj_key_.get_range().end(); }
    bytes64_t remaining_bytes() const noexcept
    {
        return obj_key_.get_range().len() - read_bytes_;
    }
    bool finished() const noexcept
    {
        return read_bytes_ == obj_key_.get_range().len();
    }
    bool valid() const noexcept { return read_bytes_ != invalid_value; }
    explicit operator bool() const noexcept { return valid(); }
};

std::ostream& operator<<(std::ostream& os,
                         const read_transaction& rhs) noexcept;

} // namespace detail
} // namespace cache
