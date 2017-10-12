#pragma once

namespace xutils
{
class io_buff_reader;
class io_buff_it;

/// A multiple reader/consumer, single writer/producer memory buffer.
/// Provides an interface to a circular queue of fixed size io_buff_block's.
class io_buff
{
private:
    friend io_buff_reader;
    friend io_buff_it;

    using list_hook_t = boost::intrusive::list_base_hook<
        boost::intrusive::link_mode<boost::intrusive::safe_link>>;

    struct mem_block : public list_hook_t
    {
        char* ptr_;

        explicit mem_block(char* ptr) noexcept : ptr_(ptr) {}
        static mem_block* alloc(uint32_t size) noexcept;
    };

    using buf_off_t = uint32_t;
    using block_list_t =
        boost::intrusive::list<mem_block,
                               boost::intrusive::constant_time_size<true>>;

    static constexpr buf_off_t max_off = UINT32_MAX;

    block_list_t blocks_;
    const uint32_t block_size_;
    buf_off_t wr_offset_;
    std::vector<buf_off_t> rdr_offsets_;

public:
    using block_t = x3me::mem_utils::array_view<char>;

public:
    explicit io_buff(uint32_t block_size) noexcept;
    ~io_buff() noexcept;

    io_buff(const io_buff&) = delete;
    io_buff& operator=(const io_buff&) = delete;
    io_buff(io_buff&&) = delete;
    io_buff& operator=(io_buff&&) = delete;

    /// Registers new reader to the io_buff.
    /// The reader will receive the minimum current read offset from the buffer.
    bool register_reader(io_buff_reader& rdr) noexcept;
    // Un-registering all of the readers is not allowed, because we can't
    // tell what part is written anymore.
    void unregister_reader(io_buff_reader& rdr) noexcept;
    /// Returns the total capacity of the buffer
    uint32_t capacity() const noexcept;
    /// Expand buffer with count bytes.
    void expand_with(uint32_t bytes) noexcept;
    /// Expand buffer with count bytes.
    uint32_t bytes_avail_wr() const noexcept;
    /// Add (count) bytes to the use/read section, increases the use section of
    /// the buffer.
    void commit(uint32_t bytes) noexcept;
    /// Returns an iterator to the first block of the buffer.
    io_buff_it begin() noexcept;
    /// Returns an iterator to the block following the last block of the buffer.
    io_buff_it end() noexcept;

    /// Returns the buffer internal block size set upon construction.
    uint32_t block_size() const noexcept { return block_size_; }

private:
    void next_it(io_buff_it& it) noexcept;
    block_t get_block(const io_buff_it& it) noexcept;

    buf_off_t rdr_min_offset() const noexcept;
    buf_off_t next_rdr_offset_or(buf_off_t off, buf_off_t def) const noexcept;
};

////////////////////////////////////////////////////////////////////////////////

class io_buff_it : public boost::iterator_facade<io_buff_it, io_buff::block_t,
                                                 boost::forward_traversal_tag,
                                                 io_buff::block_t>
{
    friend class io_buff;
    friend class boost::iterator_core_access;

    io_buff* buff_               = nullptr;
    io_buff::mem_block* block_   = nullptr;
    io_buff::buf_off_t curr_off_ = io_buff::max_off;
    uint32_t remaining_bytes_    = 0;

public:
    io_buff_it() noexcept = default;
    ~io_buff_it() noexcept = default;

private:
    io_buff_it(io_buff* bf, io_buff::mem_block* bl, io_buff::buf_off_t off,
               uint32_t rem) noexcept : buff_(bf),
                                        block_(bl),
                                        curr_off_(off),
                                        remaining_bytes_(rem)
    {
    }

    void increment() noexcept { buff_->next_it(*this); }

    io_buff::block_t dereference() const noexcept
    {
        return buff_->get_block(*this);
    }

    bool equal(const io_buff_it& rhs) const
    {
        return (buff_ == rhs.buff_) && (block_ == rhs.block_) &&
               (curr_off_ == rhs.curr_off_) &&
               (remaining_bytes_ == rhs.remaining_bytes_);
    }
};

} // end of xutils
