#pragma once

#include "object_key.h"

namespace cache
{
namespace detail
{

class write_transaction
{
    static constexpr auto invalid_value = bytes64_t(-1);

    fs_node_key_t fs_node_key_;
    range rng_;
    bytes64_t written_ = invalid_value;

public:
    write_transaction() noexcept;
    write_transaction(const fs_node_key_t& key, const range& rng) noexcept;
    ~write_transaction() noexcept;

    write_transaction(const write_transaction&) = delete;
    write_transaction& operator=(const write_transaction&) = delete;

    write_transaction(write_transaction&& rhs) noexcept;
    write_transaction& operator=(write_transaction&& rhs) noexcept;

    void inc_written(bytes64_t bytes) noexcept;

    void invalidate() noexcept;

    object_key_view obj_key() const noexcept
    {
        return object_key_view{fs_node_key_, rng_};
    }
    const fs_node_key_t& fs_node_key() const noexcept { return fs_node_key_; }
    const range& get_range() const noexcept { return rng_; }
    bytes64_t written() const noexcept { return written_; }
    bytes64_t curr_offset() const noexcept { return rng_.beg() + written_; }
    bytes64_t remaining_bytes() const noexcept { return rng_.len() - written_; }
    bool finished() const noexcept { return written_ == rng_.len(); }
    bool valid() const noexcept { return written_ != invalid_value; }
    explicit operator bool() const noexcept { return valid(); }
};

std::ostream& operator<<(std::ostream& os,
                         const write_transaction& rhs) noexcept;

} // namespace detail
} // namespace cache
