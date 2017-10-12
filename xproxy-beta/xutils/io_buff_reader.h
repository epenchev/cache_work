#pragma once

namespace xutils
{
class io_buff;
class io_buff_reader_it;
/// A reader for the io_buff.
/// It's a consumer/reader registered to a given io_buff that can read from it.
/// Multiple consumers/readers can be registered with a single io_buff.
class io_buff_reader
{
    friend      io_buff;
    friend      io_buff_reader_it;

    using rdr_idx_t = uint8_t;

    io_buff*   buff_;
    rdr_idx_t  rdr_idx_;

public:
    using block_t  = x3me::mem_utils::array_view<const char>;

public:
    io_buff_reader() noexcept;
    ~io_buff_reader() noexcept;

    io_buff_reader(io_buff_reader&& rhs) noexcept;
    io_buff_reader& operator=(io_buff_reader&& rhs) noexcept;

    io_buff_reader(const io_buff_reader&) = delete;
    io_buff_reader& operator=(const io_buff_reader&) = delete;

    /// Returns the current bytes which can be read from the reader.
    uint32_t bytes_avail() const noexcept;
    /// Decrease the use/read section of the buffer with (count) bytes.
    void consume(uint32_t bytes) noexcept;
    /// Returns an iterator to the first readable block of the buffer.
    io_buff_reader_it begin() const noexcept;
    /// Returns an iterator to the block following the last readable block of the buffer.
    io_buff_reader_it end() const noexcept;

private:
    void next_it(io_buff_reader_it& it) const noexcept;
    block_t get_block(const io_buff_reader_it& it) const noexcept;
};

////////////////////////////////////////////////////////////////////////////////

class io_buff_reader_it
    : public boost::iterator_facade<io_buff_reader_it, io_buff_reader::block_t,
                                    boost::forward_traversal_tag,
                                    io_buff_reader::block_t>
{
    friend class io_buff_reader;
    friend class boost::iterator_core_access;

    // Yep, the block_ thing is really ugly and unsafe.
    // It's done just to avoid header inclusion here.
    const io_buff_reader* rdr_ = nullptr;
    const void* block_         = nullptr; // io_buff::mem_block
    uint32_t curr_off_         = -1;
    uint32_t remaining_bytes_  = 0;

public:
    io_buff_reader_it() noexcept = default;
    ~io_buff_reader_it() noexcept = default;

private:
    io_buff_reader_it(const io_buff_reader* rdr, const void* block,
                      uint32_t off, uint32_t rem) noexcept
        : rdr_(rdr),
          block_(block),
          curr_off_(off),
          remaining_bytes_(rem)
    {
    }

    void increment() noexcept { rdr_->next_it(*this); }

    io_buff_reader::block_t dereference() const noexcept
    {
        return rdr_->get_block(*this);
    }

    bool equal(const io_buff_reader_it& rhs) const
    {
        return (rdr_ == rhs.rdr_) && (block_ == rhs.block_) &&
               (curr_off_ == rhs.curr_off_) &&
               (remaining_bytes_ == rhs.remaining_bytes_);
    }
};

} // xutils


