#pragma once

#include "hdr_value_pos.h"

namespace http
{
namespace detail
{

template <size_t KeyStoreLen>
hdr_values_store<KeyStoreLen>::hdr_values_store() noexcept
{
}

template <size_t KeyStoreLen>
hdr_values_store<KeyStoreLen>::~hdr_values_store() noexcept
{
}

template <size_t KeyStoreLen>
hdr_values_store<KeyStoreLen>::hdr_values_store(hdr_values_store&& rhs) noexcept
    : values_store_(std::move(rhs.values_store_)),
      curr_value_pos_(rhs.curr_value_pos_),
      curr_key_len_(rhs.curr_key_len_),
      curr_key_(rhs.curr_key_)
{
    rhs.curr_value_pos_ = 0;
    rhs.curr_key_len_   = 0;
}

template <size_t KeyStoreLen>
hdr_values_store<KeyStoreLen>& hdr_values_store<KeyStoreLen>::
operator=(hdr_values_store&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        values_store_   = std::move(rhs.values_store_);
        curr_value_pos_ = rhs.curr_value_pos_;
        curr_key_len_   = rhs.curr_key_len_;
        curr_key_       = rhs.curr_key_;

        rhs.curr_value_pos_ = 0;
        rhs.curr_key_len_   = 0;
    }
    return *this;
}

template <size_t KeyStoreLen>
void hdr_values_store<KeyStoreLen>::start_key() noexcept
{
    curr_key_len_ = 0;
}

template <size_t KeyStoreLen>
bool hdr_values_store<KeyStoreLen>::append_key(const char* d,
                                               bytes32_t s) noexcept
{
    const bool res = ((curr_key_len_ + bytes64_t(s)) <= max_key_len);
    // Append as much as the key buffer allows, and count the others.
    if (curr_key_len_ < max_key_len)
    {
        if (curr_key_len_ < curr_key_.size())
        {
            const auto len =
                std::min<size_t>(curr_key_.size() - curr_key_len_, s);
            ::memcpy(&curr_key_[curr_key_len_], d, len);
        }
        curr_key_len_ += std::min(max_key_len - curr_key_len_, s);
    }
    return res;
}

template <size_t KeyStoreLen>
typename hdr_values_store<KeyStoreLen>::key_info
hdr_values_store<KeyStoreLen>::current_key() const noexcept
{
    const auto key_len = std::min<size_t>(curr_key_len_, curr_key_.size());
    return key_info{string_view_t{curr_key_.data(), key_len},
                    (key_len == curr_key_len_)};
}

template <size_t KeyStoreLen>
bool hdr_values_store<KeyStoreLen>::append_value(const char* d,
                                                 bytes32_t s) noexcept
{
    if ((values_store_.size() + bytes64_t(s)) <= max_store_size)
    {
        values_store_.append(d, s);
        return true;
    }
    return false;
}

template <size_t KeyStoreLen>
void hdr_values_store<KeyStoreLen>::remove_current_value() noexcept
{
    // Remove the non-committed current value
    values_store_.erase(curr_value_pos_);
}

template <size_t KeyStoreLen>
void hdr_values_store<KeyStoreLen>::commit_current_value() noexcept
{
    curr_value_pos_ = values_store_.size();
}

template <size_t KeyStoreLen>
hdr_value_pos hdr_values_store<KeyStoreLen>::current_value_pos() const noexcept
{
    return hdr_value_pos(curr_value_pos_, values_store_.size());
}

template <size_t KeyStoreLen>
string_view_t hdr_values_store<KeyStoreLen>::current_value_view() const noexcept
{
    return string_view_t(values_store_.data() + curr_value_pos_,
                         values_store_.size() - curr_value_pos_);
}

template <size_t KeyStoreLen>
string_view_t
hdr_values_store<KeyStoreLen>::value_pos_to_view(const hdr_value_pos& pos) const
    noexcept
{
    assert(pos.end_ <= values_store_.size());
    assert(pos.beg_ <= pos.end_);
    return string_view_t(values_store_.data() + pos.beg_, pos.end_ - pos.beg_);
}

template <size_t KeyStoreLen>
string_view_t hdr_values_store<KeyStoreLen>::all_values() const noexcept
{
    return string_view_t{values_store_.data(), values_store_.size()};
}

} // namespace detail
} // namespace http
