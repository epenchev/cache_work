#pragma once

namespace cache
{
namespace detail
{

// Custom deleter is used instead of &free, because it allows empty
// base optimization and thus the alligned_data_ptr size will remain 8 bytes.
struct free_delete
{
    void operator()(void* p) noexcept { ::free(p); }
};
using aligned_data_ptr_t = std::unique_ptr<uint8_t[], free_delete>;

aligned_data_ptr_t alloc_page_aligned(size_t size) noexcept;

} // namespace detail
} // namespace cache
