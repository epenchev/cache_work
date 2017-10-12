#pragma once

namespace http
{
namespace detail
{
class hdr_value_pos;

template <size_t KeyStoreLen>
class hdr_values_store
{
public:
    enum : bytes32_t
    {
        key_store_len = KeyStoreLen,
        // Won't store values if the store heap size becomes larger than this.
        max_store_size = 1_KB,
        // Don't process insanely long keys. There must be something wrong
        // with them.
        max_key_len = 10_KB,
    };

private:
    static_assert(key_store_len <= 32, "Keep some sane limit");
    static_assert(key_store_len < max_key_len, "");

    boost::container::string values_store_;
    bytes32_t curr_value_pos_ = 0;
    bytes32_t curr_key_len_   = 0;
    std::array<char, key_store_len> curr_key_;

public:
    hdr_values_store() noexcept;
    ~hdr_values_store() noexcept;

    hdr_values_store(hdr_values_store&& rhs) noexcept;
    hdr_values_store& operator=(hdr_values_store&& rhs) noexcept;

    hdr_values_store(const hdr_values_store&) = delete;
    hdr_values_store& operator=(const hdr_values_store&) = delete;

    void start_key() noexcept;
    // Returns false if the max_key_len limit has been reached
    bool append_key(const char* d, bytes32_t s) noexcept;
    // Returns valid key if the current key fits in the key storage.
    // The returned key is not valid after the object is moved from.
    struct key_info
    {
        string_view_t key_;
        bool full_;
    };
    key_info current_key() const noexcept;

    // Returns false if the max_store_size limit has been reached
    bool append_value(const char* d, bytes32_t s) noexcept;
    void remove_current_value() noexcept;
    void commit_current_value() noexcept;
    hdr_value_pos current_value_pos() const noexcept;
    string_view_t current_value_view() const noexcept;
    string_view_t value_pos_to_view(const hdr_value_pos& pos) const noexcept;

    string_view_t all_values() const noexcept;
};

} // namespace detail
} // namespace http
