#include "precompiled.h"
#include "write_transaction.h"

namespace cache
{
namespace detail
{

write_transaction::write_transaction() noexcept
{
}

write_transaction::write_transaction(const fs_node_key_t& key,
                                     const range& rng) noexcept
    : fs_node_key_(key),
      rng_(rng),
      written_(0)
{
}

write_transaction::~write_transaction() noexcept
{
}

write_transaction::write_transaction(write_transaction&& rhs) noexcept
    : fs_node_key_(std::move(rhs.fs_node_key_)),
      rng_(std::move(rhs.rng_)),
      written_(std::move(rhs.written_))
{
    rhs.invalidate();
}

write_transaction& write_transaction::
operator=(write_transaction&& rhs) noexcept
{
    if (X3ME_LIKELY(&rhs != this))
    {
        X3ME_ASSERT(!valid(), "Must not overwrite valid/unfinished write");

        fs_node_key_ = std::move(rhs.fs_node_key_);
        rng_         = std::move(rhs.rng_);
        written_     = std::move(rhs.written_);

        rhs.invalidate();
    }
    return *this;
}

void write_transaction::inc_written(bytes64_t bytes) noexcept
{
    X3ME_ASSERT(valid(), "Must not call the method on invalid object");
    // The assert is in this way because written_ + bytes could overflow
    // and we want to assert on such cases also.
    X3ME_ASSERT((rng_.len() - written_) >= bytes, "Overflow in the read_bytes");
    written_ += bytes;
}

void write_transaction::invalidate() noexcept
{
    written_ = invalid_value;
}

std::ostream& operator<<(std::ostream& os,
                         const write_transaction& rhs) noexcept
{
    os << "{fs_key: " << rhs.fs_node_key() << ", rng: " << rhs.get_range();
    if (rhs.valid())
        os << ", written_bytes: " << rhs.written() << '}';
    else
        os << ", written_bytes: invalid}";
    return os;
}

} // namespace detail
} // namespace cache
