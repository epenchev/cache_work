#pragma once

#include "hdr_values_store.h"

namespace http
{
namespace detail
{

class hdr_value_pos;

// Enable lazy logging
template <typename KeyStoreLen>
class hdr_value_logger
{
    const hdr_values_store<KeyStoreLen>& store_;
    const hdr_value_pos& pos_;

public:
    hdr_value_logger(const hdr_values_store<KeyStoreLen>& s,
                     const hdr_value_pos& p) noexcept : store_(s),
                                                        pos_(p)
    {
    }

    template <typename KSLen>
    friend std::ostream& operator<<(std::ostream& os,
                                    const hdr_value_logger<KSLen>& rhs) noexcept
    {
        return os << rhs.store_.value_pos_to_view(rhs.pos_);
    }
};

template <typename KeyStoreLen>
auto log_hdr_val(const hdr_values_store<KeyStoreLen>& s,
                 const hdr_value_pos& p) noexcept
{
    return hdr_value_logger<KeyStoreLen>(s, p);
}

} // namespace detail
} // namespace http
