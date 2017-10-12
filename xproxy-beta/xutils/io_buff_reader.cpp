#include "precompiled.h"
#include "io_buff_reader.h"
#include "io_buff.h"

namespace xutils
{

io_buff_reader::io_buff_reader() noexcept : buff_(nullptr), rdr_idx_(0)
{
}

io_buff_reader::~io_buff_reader() noexcept
{
    if (buff_)
        buff_->unregister_reader(*this);
}

io_buff_reader::io_buff_reader(io_buff_reader&& rhs) noexcept
    : buff_(std::exchange(rhs.buff_, nullptr)),
      rdr_idx_(rhs.rdr_idx_)
{
}

io_buff_reader& io_buff_reader::operator=(io_buff_reader&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        buff_    = std::exchange(rhs.buff_, nullptr);
        rdr_idx_ = rhs.rdr_idx_;
    }
    return *this;
}

uint32_t io_buff_reader::bytes_avail() const noexcept
{
    assert(buff_ && "Reader not registered");
    const auto wr_off = buff_->wr_offset_;
    const auto rd_off = buff_->rdr_offsets_[rdr_idx_];
    if (wr_off > rd_off)
        return (wr_off - rd_off);
    else if (rd_off > wr_off)
        return (buff_->capacity() - (rd_off - wr_off));
    return 0;
}

void io_buff_reader::consume(uint32_t bytes) noexcept
{
    assert(bytes <= bytes_avail());

    auto& rd_off = buff_->rdr_offsets_[rdr_idx_];
    // Prevent 32 bit unsigned from overflow.
    const auto new_offset = rd_off + static_cast<uint64_t>(bytes);
    rd_off                = new_offset % buff_->capacity();
}

io_buff_reader_it io_buff_reader::begin() const noexcept
{
    static_assert(std::is_same<decltype(io_buff_reader_it::curr_off_),
                               io_buff::buf_off_t>::value,
                  "");
    assert(buff_ && "Reader not registered");

    io_buff_reader_it it{};
    const auto& blocks = buff_->blocks_;
    if (const auto bts_avail = bytes_avail())
    {
        const auto rd_off        = buff_->rdr_offsets_[rdr_idx_];
        const auto rd_off_blocks = rd_off / buff_->block_size_;
        assert(rd_off_blocks < blocks.size());

        it.rdr_             = this;
        it.block_           = &(*std::next(blocks.begin(), rd_off_blocks));
        it.curr_off_        = rd_off;
        it.remaining_bytes_ = bts_avail;
    }
    return it;
}

io_buff_reader_it io_buff_reader::end() const noexcept
{
    return io_buff_reader_it{};
}

////////////////////////////////////////////////////////////////////////////////

void io_buff_reader::next_it(io_buff_reader_it& it) const noexcept
{
    assert(it.remaining_bytes_ > 0);

    const auto& blocks     = buff_->blocks_;
    const auto block_size  = buff_->block_size_;
    const auto block_bytes = block_size - (it.curr_off_ % block_size);
    const auto curr_bytes  = std::min(it.remaining_bytes_, block_bytes);
    const auto block       = static_cast<const io_buff::mem_block*>(it.block_);

    if (it.remaining_bytes_ == curr_bytes)
    {
        it = end();
        return;
    }

    it.curr_off_ += curr_bytes;
    it.remaining_bytes_ -= curr_bytes;

    auto block_it = io_buff::block_list_t::s_iterator_to(*block);
    ++block_it;
    if (block_it != blocks.end())
    {
        assert(it.curr_off_ < buff_->capacity());
        it.block_ = &(*block_it);
    }
    else
    {
        assert(it.curr_off_ == buff_->capacity());
        it.curr_off_ = 0;
        it.block_    = &(*blocks.begin());
    }
}

io_buff_reader::block_t
io_buff_reader::get_block(const io_buff_reader_it& it) const noexcept
{
    const auto block_size = buff_->block_size_;
    const auto rel_off    = it.curr_off_ % block_size;
    const auto len        = std::min(block_size - rel_off, it.remaining_bytes_);
    const auto block      = static_cast<const io_buff::mem_block*>(it.block_);
    return block_t{block->ptr_ + rel_off, len};
}

} // namespace xutils
