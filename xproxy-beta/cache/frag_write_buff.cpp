#include "precompiled.h"
#include "frag_write_buff.h"

namespace cache
{
namespace detail
{

frag_write_buff::frag_write_buff(bytes32_t capacity) noexcept
    : buff_(static_cast<uint8_t*>(::malloc(capacity))),
      size_(0),
      capacity_(capacity)
{
    X3ME_ASSERT(capacity > 0, "The provided capacity is invalid");
}

frag_write_buff::frag_write_buff(frag_write_buff&& rhs) noexcept
    : buff_(std::move(rhs.buff_)),
      size_(std::exchange(rhs.size_, 0)),
      capacity_(std::exchange(rhs.capacity_, 0))
{
}

frag_write_buff& frag_write_buff::operator=(frag_write_buff&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        buff_     = std::move(rhs.buff_);
        size_     = std::exchange(rhs.size_, 0);
        capacity_ = std::exchange(rhs.capacity_, 0);
    }
    return *this;
}

void frag_write_buff::commit(bytes32_t size) noexcept
{
    X3ME_ASSERT(bytes64_t(size_) + size <= capacity_, "Committed size too big");
    size_ += size;
}

} // namespace detail
} // namespace cache
