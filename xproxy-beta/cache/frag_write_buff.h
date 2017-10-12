#pragma once

namespace cache
{
namespace detail
{

class frag_write_buff
{
    struct free_delete
    {
        void operator()(void* p) noexcept { ::free(p); }
    };
    using buff_ptr_t = std::unique_ptr<uint8_t[], free_delete>;

    buff_ptr_t buff_;
    bytes32_t size_     = 0;
    bytes32_t capacity_ = 0;

public:
    frag_write_buff() noexcept = default;
    explicit frag_write_buff(bytes32_t capacity) noexcept;
    frag_write_buff(frag_write_buff&& rhs) noexcept;
    frag_write_buff& operator=(frag_write_buff&& rhs) noexcept;

    using buffer_t = x3me::mem_utils::array_view<uint8_t>;
    buffer_t buff() noexcept
    {
        return buffer_t{buff_.get() + size_, capacity_ - size_};
    }

    void commit(bytes32_t size) noexcept;

    void clear() noexcept { size_ = 0; }

    const uint8_t* begin() const noexcept { return buff_.get(); }
    const uint8_t* end() const noexcept { return buff_.get() + size_; }
    const uint8_t* data() const noexcept { return buff_.get(); }
    bytes32_t size() const noexcept { return size_; }
    bytes32_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }
    bool full() const noexcept { return size_ == capacity_; }
};

} // namespace detail
} // namespace cache
