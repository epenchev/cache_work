#include "precompiled.h"
#include "read_transaction.h"

namespace cache
{
namespace detail
{

read_transaction::read_transaction(const object_key& obj_key) noexcept
    : obj_key_(obj_key),
      read_bytes_(0)
{
}

read_transaction::~read_transaction() noexcept
{
}

read_transaction::read_transaction(read_transaction&& rhs) noexcept
    : obj_key_(std::move(rhs.obj_key_)),
      read_bytes_(std::move(rhs.read_bytes_))
{
    rhs.invalidate();
}

read_transaction& read_transaction::operator=(read_transaction&& rhs) noexcept
{
    if (X3ME_LIKELY(&rhs != this))
    {
        X3ME_ASSERT(!valid(), "Must not overwrite valid/unfinished read");

        obj_key_    = std::move(rhs.obj_key_);
        read_bytes_ = std::move(rhs.read_bytes_);

        rhs.invalidate();
    }
    return *this;
}

void read_transaction::inc_read_bytes(bytes64_t bytes) noexcept
{
    X3ME_ASSERT(valid(), "Must not call the method on invalid object");
    // The assert is in this way because written_ + bytes could overflow
    // and we want to assert on such cases also.
    X3ME_ASSERT((get_range().len() - read_bytes_) >= bytes,
                "Overflow in the read_bytes");
    read_bytes_ += bytes;
}

void read_transaction::invalidate() noexcept
{
    read_bytes_ = invalid_value;
}

std::ostream& operator<<(std::ostream& os, const read_transaction& rhs) noexcept
{
    os << "{fs_key: " << rhs.fs_node_key() << ", rng: " << rhs.get_range();
    if (rhs.valid())
        os << ", read_bytes: " << rhs.read_bytes() << '}';
    else
        os << ", read_bytes: invalid}";
    return os;
}

} // namespace detail
} // namespace cache
