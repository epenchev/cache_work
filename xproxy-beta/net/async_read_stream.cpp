#include "precompiled.h"
#include "async_read_stream.h"

namespace net
{

async_read_stream::async_read_stream() noexcept : valid_(false)
{
    ::memset(&storage_, 0, sizeof(storage_));
}

async_read_stream::async_read_stream(async_read_stream&& rhs) noexcept
    : valid_(std::exchange(rhs.valid_, false))
{
    ::memcpy(&storage_, &rhs.storage_, sizeof(storage_));
    ::memset(&rhs.storage_, 0, sizeof(storage_));
}

async_read_stream& async_read_stream::
operator=(async_read_stream&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        if (valid_)
            impl()->~implementation();
        valid_ = std::exchange(rhs.valid_, false);
        ::memcpy(&storage_, &rhs.storage_, sizeof(storage_));
        ::memset(&rhs.storage_, 0, sizeof(storage_));
    }
    return *this;
}

async_read_stream::~async_read_stream() noexcept
{
    if (valid_)
        impl()->~implementation();
}

} // namespace net
