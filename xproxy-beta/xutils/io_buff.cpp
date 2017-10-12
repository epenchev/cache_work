#include "precompiled.h"
#include "io_buff.h"
#include "io_buff_reader.h"

namespace xutils
{

constexpr io_buff::buf_off_t io_buff::max_off;

io_buff::mem_block* io_buff::mem_block::alloc(uint32_t size) noexcept
{
    auto mem = static_cast<char*>(::malloc(sizeof(io_buff::mem_block) + size));
    assert(mem != nullptr);
    char* p = mem + sizeof(io_buff::mem_block);
    return new (mem) io_buff::mem_block(p);
}

io_buff::io_buff(uint32_t block_size) noexcept : block_size_(block_size),
                                                 wr_offset_(0)
{
}

io_buff::~io_buff() noexcept
{
    blocks_.clear_and_dispose([](io_buff::mem_block* b)
                              {
                                  b->~mem_block();
                                  ::free(b);
                              });
}

bool io_buff::register_reader(io_buff_reader& rdr) noexcept
{
    assert(!rdr.buff_ && "Reader already registered");

    const auto min_off = rdr_min_offset();
    auto it = std::find(rdr_offsets_.begin(), rdr_offsets_.end(), max_off);
    if (it == rdr_offsets_.end())
    {
        constexpr auto max_rdrs =
            std::numeric_limits<io_buff_reader::rdr_idx_t>::max();
        if (rdr_offsets_.size() < max_rdrs)
        {
            rdr_offsets_.push_back(min_off);
            it = rdr_offsets_.end() - 1;
        }
        else
            return false;
    }
    else
    {
        *it = min_off;
    }
    rdr.buff_    = this;
    rdr.rdr_idx_ = it - rdr_offsets_.begin();
    return true;
}

void io_buff::unregister_reader(io_buff_reader& rdr) noexcept
{
    assert(rdr.buff_ && "Reader not registered");

    rdr.buff_                  = nullptr;
    rdr_offsets_[rdr.rdr_idx_] = max_off;
}

uint32_t io_buff::capacity() const noexcept
{
    return blocks_.size() * block_size_;
}

void io_buff::expand_with(uint32_t bytes) noexcept
{
    const uint64_t blk_cnt  = x3me::math::divide_round_up(bytes, block_size_);
    const auto blocks_bytes = blk_cnt * block_size_;
    // This is not a precise assert bug ...
    assert((capacity() + blocks_bytes < max_off) && "Expansion too big");

    auto do_expand = [this](auto pos, uint32_t expanded_size, uint32_t bytes)
    {
        for (; expanded_size < bytes; expanded_size += block_size_)
            pos = blocks_.insert(pos, *mem_block::alloc(block_size_));
        return expanded_size;
    };

    const auto rd_first = next_rdr_offset_or(wr_offset_, 0);

    // also checks if there are no blocks in buffer
    if (rd_first <= wr_offset_)
    {
        do_expand(blocks_.end(), 0, bytes);
        return;
    }

    const auto wr_off_blks = wr_offset_ / block_size_;
    const auto wr_off_end = (wr_off_blks + 1) * block_size_;
    assert((wr_off_blks < blocks_.size()) && "Wrong write_offset");
    auto wr_it = std::next(blocks_.begin(), wr_off_blks);

    const auto rdr_same_block =
        (wr_offset_ < rd_first) && (rd_first < wr_off_end);
    uint32_t expanded_size = 0;
    if (rdr_same_block)
    { // There is a reader after the writer and in the same
        // block.
        // We need to move the writer in a separate block and so that
        // we can add new blocks after it.
        const auto len_to_move = wr_offset_ - (wr_off_blks * block_size_);
        auto blk = mem_block::alloc(block_size_);
        ::memcpy(blk->ptr_, wr_it->ptr_, len_to_move);
        wr_it         = blocks_.insert(wr_it, *blk);
        expanded_size = block_size_;
    }
    expanded_size = do_expand(++wr_it, expanded_size, bytes);
    assert((expanded_size == blocks_bytes) && "The logic here is wrong");

    // The last thing we need to do is to readjust all reader offsets which
    // are after the write_offset_. Note that the write_offset_ doesn't change.
    for (auto& off : rdr_offsets_)
    {
        off += (off > wr_offset_) * expanded_size;
    }
}

uint32_t io_buff::bytes_avail_wr() const noexcept
{
    if (blocks_.empty())
        return 0;
    const auto wr_off   = wr_offset_;
    const auto next_off = next_rdr_offset_or(wr_off, 0);
    if (next_off > wr_off)
        return (next_off - wr_off) - 1;
    return (capacity() - (wr_off - next_off) - 1);
}

void io_buff::commit(uint32_t bytes) noexcept
{
    assert((bytes <= bytes_avail_wr()) && "Commit too much");

    // Prevent 32 bit unsigned from overflow.
    const auto new_offset = wr_offset_ + static_cast<uint64_t>(bytes);
    wr_offset_            = new_offset % capacity();
}

io_buff_it io_buff::begin() noexcept
{
    io_buff_it it{};
    if (const auto bytes = bytes_avail_wr())
    {
        const auto wr_off_blocks = wr_offset_ / block_size_;
        assert(wr_off_blocks < blocks_.size());

        it.buff_            = this;
        it.block_           = &(*std::next(blocks_.begin(), wr_off_blocks));
        it.curr_off_        = wr_offset_;
        it.remaining_bytes_ = bytes;
    }
    return it;
}

io_buff_it io_buff::end() noexcept
{
    return io_buff_it{};
}

////////////////////////////////////////////////////////////////////////////////

void io_buff::next_it(io_buff_it& it) noexcept
{
    assert(it.remaining_bytes_ > 0);

    const auto block_bytes = block_size_ - (it.curr_off_ % block_size_);
    const auto curr_bytes  = std::min(it.remaining_bytes_, block_bytes);

    if (it.remaining_bytes_ == curr_bytes)
    {
        it = end();
        return;
    }

    it.curr_off_ += curr_bytes;
    it.remaining_bytes_ -= curr_bytes;

    auto block_it = block_list_t::s_iterator_to(*it.block_);
    ++block_it;
    if (block_it != blocks_.end())
    {
        assert(it.curr_off_ < capacity());
        it.block_ = &(*block_it);
    }
    else
    {
        assert(it.curr_off_ == capacity());
        it.curr_off_ = 0;
        it.block_    = &(*blocks_.begin());
    }
}

io_buff::block_t io_buff::get_block(const io_buff_it& it) noexcept
{
    assert(it.block_ && "iterator has no block");
    const auto rel_off = it.curr_off_ % block_size_;
    const auto len     = std::min(block_size_ - rel_off, it.remaining_bytes_);
    return block_t{it.block_->ptr_ + rel_off, len};
}

io_buff::buf_off_t io_buff::rdr_min_offset() const noexcept
{
    return !rdr_offsets_.empty()
               ? *std::min_element(rdr_offsets_.begin(), rdr_offsets_.end())
               : 0;
}

io_buff::buf_off_t io_buff::next_rdr_offset_or(buf_off_t off,
                                               buf_off_t def) const noexcept
{
    buf_off_t minoff  = max_off;
    buf_off_t nextoff = max_off;
    for (buf_off_t rdr_off : rdr_offsets_)
    {
        if ((off < rdr_off) && (rdr_off < nextoff) /* && (rdr_off != max_off)*/)
            nextoff = rdr_off;
        if ((rdr_off < minoff) && (rdr_off <= off))
            minoff = rdr_off;
    }
    return (nextoff != max_off) ? nextoff
                                : ((minoff != max_off) ? minoff : def);
}

} // namespace xutils
