#pragma once

namespace cache
{
namespace detail
{

struct aio_data
{
    uint8_t* buf_   = nullptr;
    bytes64_t offs_ = 0;
    bytes32_t size_ = 0;
};

} // namespace detail
} // namespace cache
