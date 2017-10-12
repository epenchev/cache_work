#include "precompiled.h"
#include "aligned_data_ptr.h"

namespace cache
{
namespace detail
{

static bytes32_t page_size = []
{
    const auto ret = x3me::sys_utils::memory_page_size();
    X3ME_ENFORCE(ret > 0);
    return ret;
}(); // Note the call

aligned_data_ptr_t alloc_page_aligned(size_t size) noexcept
{
    void* ptr      = nullptr;
    const auto ret = ::posix_memalign(&ptr, page_size, size);
    X3ME_ENFORCE(ret == 0, "Allocation failed");
    return aligned_data_ptr_t(static_cast<uint8_t*>(ptr));
}

} // namespace detail
} // namespace cache
