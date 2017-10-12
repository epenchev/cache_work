#include "precompiled.h"
#include "agg_writer.h"
#include "agg_write_meta.h"
#include "cache_fs_ops.h"
#include "cache_stats.h"
#include "frag_write_buff.h"
#include "memory_reader.h"
#include "object_frag_hdr.h"
#include "write_transaction.h"

namespace cache
{
namespace detail
{
template <typename Stat, typename Val>
static void inc_stat(std::atomic<Stat>& cnt, Val val) noexcept
{
    cnt.fetch_add(val, std::memory_order_release);
}

template <typename Stat>
static Stat read_stat(const std::atomic<Stat>& cnt) noexcept
{
    return cnt.load(std::memory_order_acquire);
}

////////////////////////////////////////////////////////////////////////////////
namespace awsm
{
struct state_data
{
    struct pending_data
    {
        frag_write_buff buff_;
        write_transaction trans_;
    };

    agg_write_meta evac_meta_{agg_write_meta_size};
    // TODO A possible optimization here is to use directly the
    // agg_write_block buffer and avoid additional allocation of the
    // evacuation fragment.
    aligned_data_ptr_t evac_frag_;

    pending_data pend_data_;

    volume_blocks64_t write_pos_ = volume_blocks64_t::zero();
    bool is_first_lap_           = false;

    bytes64_t wr_pos() const noexcept { return write_pos_.to_bytes(); }
};

////////////////////////////////////////////////////////////////////////////////

// clang-format off
struct ev_do_next {};
struct ev_io_begin {};
struct ev_io_done { const err_code_t* err_; };
struct ev_do_write 
{ 
    const frag_write_buff* wbuf_; 
    write_transaction* wtrans_;
    bool* res_;
};
struct ev_do_fin_write { frag_write_buff* wbuf_; write_transaction* wtrans_; };
struct ev_do_async_flush {};
struct ev_last_flush {};
// clang-format on

struct sm_impl
{
    struct on_unexpected_event
    {
        template <typename Ev>
        void operator()(agg_writer* w, const Ev& ev) noexcept;
    };

    auto operator()() const noexcept
    {
        // clang-format off
        auto is_first_lap = [](agg_writer* w)
        { 
            return w->sdata_->is_first_lap_;
        };
        auto evac_needed = [](agg_writer* w)
        { 
            return !w->sdata_->evac_meta_.empty();
        };

        auto enqueue_read_aio_op = [](agg_writer* w)
        { 
            w->enqueue_read_aio_op();
        };
        auto enqueue_write_aio_op = [](agg_writer* w)
        { 
            w->enqueue_write_aio_op();
        };
        auto begin_md_read = [](agg_writer* w){ w->begin_md_read(); };
        auto on_md_read = [](agg_writer* w, auto ev){ w->on_md_read(ev); };
        auto begin_evac = [](agg_writer* w){ w->begin_evac(); };
        auto on_evac_done = [](agg_writer* w, auto ev){ w->on_evac_done(ev); };
        auto write_pend_data = [](agg_writer* w){ w->write_pend_data(); };
        auto do_write = [](agg_writer* w, auto ev){ w->do_write(ev); };
        auto do_fin_write = [](agg_writer* w, auto ev){ w->do_fin_write(ev); };
        auto begin_flush = [](agg_writer* w){ w->begin_flush(); };
        auto on_flush_done = [](agg_writer* w, auto ev){ w->on_flush_done(ev); };
        auto do_last_flush = [](agg_writer* w){ w->do_last_flush(); };

        using namespace boost::sml;
        const auto begin_s          = "begin"_s;
        const auto async_md_read1_s = "async_md_read1"_s;
        const auto async_md_read2_s = "async_md_read2"_s;
        const auto async_evac1_s    = "async_evac1"_s;
        const auto async_evac2_s    = "async_evac2"_s;
        const auto async_flush1_s   = "async_flush1"_s;
        const auto async_flush2_s   = "async_flush2"_s;
        const auto wait_write_s     = "wait_write"_s;
        const auto wait_next_s      = "wait_next"_s;
        return make_transition_table(
            // First handle the aggregate fragment metadata reading
            *begin_s + event<ev_do_next>[is_first_lap] / write_pend_data = 
                                                                wait_write_s,
            begin_s + event<ev_do_next>[!is_first_lap] / enqueue_read_aio_op = 
                                                            async_md_read1_s,
            async_md_read1_s + event<ev_io_begin> / begin_md_read =
                                                            async_md_read2_s,
            async_md_read2_s + event<ev_io_done> / on_md_read = wait_next_s,
            // Next handle evacuation one fragment at time.
            // "Loop" until all needed fragments are evacuated.
            wait_next_s + event<ev_do_next>[evac_needed] / enqueue_read_aio_op = 
                                                                async_evac1_s,
            async_evac1_s + event<ev_io_begin> / begin_evac = async_evac2_s,
            async_evac2_s + event<ev_io_done> / on_evac_done = wait_next_s,
            // Try to write pending, current and final. If we get out of space
            // the given operation will enqueue a deferred flush event and
            // we start async flush operation. We start from the beginning
            // once the flush is done.
            wait_next_s + event<ev_do_next>[!evac_needed] / write_pend_data = 
                                                                wait_write_s,
            wait_write_s + event<ev_do_write> / do_write = wait_write_s,
            wait_write_s + event<ev_do_fin_write> / do_fin_write = wait_write_s,
            wait_write_s + event<ev_do_async_flush> / enqueue_write_aio_op =
                                                                async_flush1_s,
            async_flush1_s + event<ev_io_begin> / begin_flush = async_flush2_s,
            async_flush2_s + event<ev_io_done> / on_flush_done = begin_s,
            // Do final flush unconditionally, to simplify things
            *"wait_last_flush"_s + event<ev_last_flush> / do_last_flush = X
            );
        // clang-format on
    }
};

template <typename E>
auto event_type() // Gives the event type string
{
    return __PRETTY_FUNCTION__;
}

class sm : private boost::sml::sm<sm_impl>
{
    using base_t           = boost::sml::sm<sm_impl>;
    using deferred_event_t = boost::variant<boost::blank, ev_do_async_flush>;

    static constexpr auto idx_no_event = 0;
    struct process_defr_ev : boost::static_visitor<>
    {
        sm* sm_;

        explicit process_defr_ev(sm* s) : sm_(s) {}
        void operator()(const boost::blank&) noexcept {}
        template <typename Ev>
        void operator()(const Ev& ev) noexcept
        {
            sm_->process_event(ev);
        }
    };

    deferred_event_t defr_ev_;

public:
    using base_t::base_t;

    template <typename Ev>
    void process_event(Ev ev) noexcept
    {
        const bool r = base_t::process_event(ev);
        if (X3ME_UNLIKELY(!r))
        {
            std::cerr << "BUG in Agg_writer state machine\n";
            base_t::visit_current_states(
                [](const auto& st)
                {
                    std::cerr << "\tNo transition from state " << st.c_str()
                              << " on " << event_type<Ev>() << '\n';
                });
            std::cerr << std::flush;
            ::abort();
        }
    }

    template <typename Ev>
    void enqueue_defr_event(const Ev& ev) noexcept
    {
        X3ME_ASSERT(defr_ev_.which() == idx_no_event,
                    "Only single deferred event is allowed at any time");
        defr_ev_ = ev;
    }

    void process_defr_event() noexcept
    {
        if (defr_ev_.which() != idx_no_event)
        {
            process_defr_ev proc{this};
            boost::apply_visitor(proc, defr_ev_);
            defr_ev_ = boost::blank{};
        }
    }
};

} // namespace awsm
////////////////////////////////////////////////////////////////////////////////

agg_writer::agg_writer(volume_blocks64_t write_pos, uint64_t write_lap) noexcept
    : sm_(this)
{
    sdata_->write_pos_    = write_pos;
    sdata_->is_first_lap_ = (write_lap == 0);
}

agg_writer::~agg_writer() noexcept
{
    XLOG_INFO(disk_tag, "Destroy agg_writer {}", log_ptr(this));
}

void agg_writer::start(non_owner_ptr_t<cache_fs_ops> fso) noexcept
{
    XLOG_DEBUG(
        disk_tag,
        "Create agg_writer {}. FS '{}'. Wr_pos {} bytes. Is_first_lap {}",
        log_ptr(this), fso->vol_path(), sdata_->wr_pos(),
        sdata_->is_first_lap_);
    fs_ops_ = fso;
    sm_->process_event(awsm::ev_do_next{});
}

void agg_writer::stop_flush() noexcept
{
    sm_->process_event(awsm::ev_last_flush{});
}

bool agg_writer::write(const frag_write_buff& wbuf,
                       write_transaction& wtrans) noexcept
{
    bool res = false;
    sm_->process_event(awsm::ev_do_write{&wbuf, &wtrans, &res});
    sm_->process_defr_event();
    return res;
}

void agg_writer::final_write(frag_write_buff&& wbuf,
                             write_transaction&& wtrans) noexcept
{
    sm_->process_event(awsm::ev_do_fin_write{&wbuf, &wtrans});
    sm_->process_defr_event();
}

void agg_writer::get_stats(stats_fs_wr& swr) noexcept
{
    swr.written_meta_size_        = read_stat(stats_.written_meta_size_);
    swr.wasted_meta_size_         = read_stat(stats_.wasted_meta_size_);
    swr.written_data_size_        = read_stat(stats_.written_data_size_);
    swr.wasted_data_size_         = read_stat(stats_.wasted_data_size_);
    swr.cnt_block_meta_read_ok_   = read_stat(stats_.cnt_block_meta_read_ok_);
    swr.cnt_block_meta_read_err_  = read_stat(stats_.cnt_block_meta_read_err_);
    swr.cnt_evac_entries_checked_ = read_stat(stats_.cnt_evac_entries_checked_);
    swr.cnt_evac_entries_todo_    = read_stat(stats_.cnt_evac_entries_todo_);
    swr.cnt_evac_entries_ok_      = read_stat(stats_.cnt_evac_entries_ok_);
    swr.cnt_evac_entries_err_     = read_stat(stats_.cnt_evac_entries_err_);
}

////////////////////////////////////////////////////////////////////////////////

void agg_writer::exec() noexcept
{
    X3ME_ASSERT(false, "Must not be called. We do only IO here");
}

non_owner_ptr_t<const aio_data> agg_writer::on_begin_io_op() noexcept
{
    sm_->process_event(awsm::ev_io_begin{});
    if (aio_op_ == aio_op::write)
    {
        // This is the way for us to ensure that there are no
        // readers started to physically access the write disk area before the
        // corresponding evacuation of the current write block.
        // If we can take the lock this means that there are no readers.
        fs_ops_->vmtx_wait_disk_readers();
    }
    return &aio_data_;
}

void agg_writer::on_end_io_op(const err_code_t& err) noexcept
{
    sm_->process_event(awsm::ev_io_done{&err});
    sm_->process_defr_event();
    sm_->process_event(awsm::ev_do_next{});
    sm_->process_defr_event();
}

////////////////////////////////////////////////////////////////////////////////
// It's important that all aio tasks here are pushed at the beginning of
// the aio_service queue. This way we make the other writes wait while we
// evacuate or write the aggregation buffer to the disk.

void agg_writer::enqueue_read_aio_op() noexcept
{
    aio_op_ = aio_op::read;
    fs_ops_->aios_push_front_write_queue(this);
}

void agg_writer::enqueue_write_aio_op() noexcept
{
    aio_op_ = aio_op::write;
    fs_ops_->aios_push_front_write_queue(this);
}

////////////////////////////////////////////////////////////////////////////////

void agg_writer::begin_md_read() noexcept
{
    // Use the metadata buffer of the aggregate block to read the
    // current write block metadata from the disk.
    const auto wpos = sdata_->wr_pos();
    auto buf        = write_block_->metadata_buff();
    aio_data_.buf_  = buf.data();
    aio_data_.size_ = buf.size();
    aio_data_.offs_ = wpos;
    aio_op_ = aio_op::read;
    XLOG_DEBUG(disk_tag, "Begin_MD_read agg_writer {}. Wr_pos {} bytes",
               log_ptr(this), wpos);
}

void agg_writer::on_md_read(const awsm::ev_io_done& ev) noexcept
{
    X3ME_ASSERT(sdata_->evac_meta_.empty(), "Wrong state logic");
    X3ME_ASSERT((aio_data_.size_ == agg_write_meta_size), "Wrong state logic");
    X3ME_ASSERT((aio_data_.offs_ == sdata_->wr_pos()), "Wrong state logic");

    if (*ev.err_)
    {
        inc_stat(stats_.cnt_block_meta_read_err_, 1);
        XLOG_FATAL(disk_tag,
                   "On_MD_read agg_writer {}. FS '{}'. Disk error "
                   "while reading aggregate block metadata. Wr_pos {}. {}",
                   log_ptr(this), fs_ops_->vol_path(), aio_data_.offs_,
                   ev.err_->message());
        fs_ops_->report_disk_error();
        // TODO Continue to work in the current write block as if nothing
        // has happened. This could lead to corruption of the data read by
        // some readers. It's better to go to the next write block in this case.
    }
    else
    {
        agg_write_meta tmp(agg_write_meta_size);
        memory_reader rdr{aio_data_.buf_, aio_data_.size_};
        if (tmp.load(rdr))
        {
            inc_stat(stats_.cnt_block_meta_read_ok_, 1);
            auto frags_meta = tmp.release_entries();
            inc_stat(stats_.cnt_evac_entries_checked_, frags_meta.size());
            if (!frags_meta.empty())
            {
                constexpr auto inc =
                    volume_blocks64_t::create_from_bytes(agg_write_meta_size);
                constexpr auto sz =
                    volume_blocks64_t::create_from_bytes(agg_write_data_size);
                fs_ops_->fsmd_rem_non_evac_frags(frags_meta,
                                                 sdata_->write_pos_ + inc, sz);
            }
            // TODO Stats for evacuated fragments
            XLOG_DEBUG(disk_tag, "On_MD_read agg_writer {}. {} entries "
                                 "for evacuation. Wr_pos {}",
                       log_ptr(this), frags_meta.size(), aio_data_.offs_);
            if (!frags_meta.empty())
            {

                inc_stat(stats_.cnt_evac_entries_todo_, frags_meta.size());
                sdata_->evac_meta_.set_entries(std::move(frags_meta));
            }
        }
        else
        {
            inc_stat(stats_.cnt_block_meta_read_err_, 1);
            XLOG_ERROR(disk_tag, "On_MD_read agg_writer {}. FS '{}'. Corrupted "
                                 "aggregate block metadata. Wr_pos {}",
                       log_ptr(this), fs_ops_->vol_path(), aio_data_.offs_);
            X3ME_ASSERT(sdata_->evac_meta_.empty(), "Wrong state logic");
            // Currently we pretend that nothing wrong happened here and
            // proceed with the aggregate and write logic.
            // However, this behavior could lead to corrupted reads, once in a
            // while, when disk reading happens in the same area as the
            // disk writing.
            // On the other hand, errors here should be pretty rare (unless we
            // have a lot of bugs) and racy reads/writes should be even rarer.
            // Thus let's first see how often we have errors here before
            // complicating the logic.
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void agg_writer::begin_evac() noexcept
{
    X3ME_ASSERT(!sdata_->evac_meta_.empty(), "Wrong state logic");
    constexpr auto max_sz = object_frag_size(object_frag_max_data_size);
    const auto entry      = sdata_->evac_meta_.begin();
    const auto sz         = object_frag_size(entry->rng().rng_size());
    const auto offs       = entry->rng().disk_offset().to_bytes();
    const auto rpos       = sdata_->wr_pos() + agg_write_meta_size;

    // The call to cache_fs_ops for removing non evacuate fragments meta
    // must have filtered out invalid range elements.
    X3ME_ENFORCE(sz <= max_sz, "Invalid range size");
    X3ME_ENFORCE(
        x3me::math::in_range(offs, offs + sz, rpos, rpos + agg_write_data_size),
        "Invalid range element");

    auto& frag_buff = sdata_->evac_frag_;
    if (!frag_buff) // Lazy allocation, only if needed
        frag_buff = alloc_page_aligned(max_sz);

    aio_data_.buf_  = frag_buff.get();
    aio_data_.size_ = sz;
    aio_data_.offs_ = offs;

    XLOG_DEBUG(disk_tag, "Begin_evac agg_writer {}. Wr_pos {} bytes. Entry {}",
               log_ptr(this), rpos - agg_write_meta_size, *entry);
}

void agg_writer::on_evac_done(const awsm::ev_io_done& ev) noexcept
{
    X3ME_ASSERT(!sdata_->evac_meta_.empty(), "Wrong state logic");
    auto e = sdata_->evac_meta_.begin();
    X3ME_ASSERT((aio_data_.offs_ == e->rng().disk_offset().to_bytes()),
                "Wrong state logic");
    X3ME_ASSERT((aio_data_.size_ == object_frag_size(e->rng().rng_size())),
                "Wrong state logic");

    if (*ev.err_)
    {
        inc_stat(stats_.cnt_evac_entries_err_, 1);
        XLOG_FATAL(disk_tag, "On_evac_done agg_writer {}. FS '{}'. Disk error "
                             "while reading object fragment. Entry {}. {}",
                   log_ptr(this), fs_ops_->vol_path(), *e, ev.err_->message());
        fs_ops_->report_disk_error();
        // TODO Continue to work in the current write block as if nothing
        // has happened. This could lead to corruption of the data read by
        // some readers. It's better to go to the next write block in this case.
    }
    else
    {
        object_frag_hdr hdr;
        const auto exp_hdr  = object_frag_hdr::create(e->key(), e->rng());
        const auto hdr_size = sizeof(object_frag_hdr);
        ::memcpy(&hdr, aio_data_.buf_, hdr_size);
        if (hdr == exp_hdr)
        {
            inc_stat(stats_.cnt_evac_entries_ok_, 1);
            const range rng{e->rng().rng_offset(), e->rng().rng_size(),
                            frag_rng};
            const agg_write_block::frag_ro_buff_t frag{
                aio_data_.buf_ + hdr_size, rng.len()};
            const auto res = fs_ops_->fsmd_add_evac_fragment(
                e->key(), rng, frag, sdata_->write_pos_, write_block_);
            XLOG_DEBUG(disk_tag, "On_evac_done agg_writer {}. Added evacuated "
                                 "frag. Key {}. Rng {}. Wr_pos {}. Res {}",
                       log_ptr(this), e->key(), rng, sdata_->wr_pos(), res);
        }
        else
        {
            inc_stat(stats_.cnt_evac_entries_err_, 1);
            XLOG_ERROR(disk_tag,
                       "On_evac_done agg_writer {}. FS '{}'. Corrupted "
                       "object fragment. Check sum doesn't match. Wr_pos {}. "
                       "Hdr {}. Exp_hdr {}. Entry {}",
                       log_ptr(this), fs_ops_->vol_path(), aio_data_.offs_, hdr,
                       exp_hdr, *e);
            // Can't do anything more here. Some reader will most likely
            // read corrupted data and will probably detect it by the header.
            // TODO We can forcefully remove the corrupted entry here!!!
        }
    }

    sdata_->evac_meta_.rem_entry(e); // Remove the processed entry

    // Assuming that evacuations would be a rare event. Let's not
    // keep memory allocated without a need.
    if (sdata_->evac_meta_.empty())
        sdata_->evac_frag_.reset();
}

////////////////////////////////////////////////////////////////////////////////

void agg_writer::write_pend_data() noexcept
{
    auto& pd = sdata_->pend_data_;
    if (!pd.buff_.empty())
    {
        X3ME_ASSERT(pd.trans_.valid(), "If we have non empty buffer, we must "
                                       "have a valid transaction too");
        XLOG_DEBUG(disk_tag, "Write_pend_data agg_writer {}. Trans {}",
                   log_ptr(this), pd.trans_);
        if (do_write_impl(pd.trans_, pd.buff_, true /*fin write*/))
        {
            finished_trans_.push_back(std::move(pd.trans_));
            pd.buff_ = frag_write_buff{};
        }
        else
        {
            // Flush the data to the disk to free meta/data space
            sm_->enqueue_defr_event(awsm::ev_do_async_flush{});
        }
    }
}

void agg_writer::do_write(const awsm::ev_do_write& ev) noexcept
{
    bool r = do_write_impl(*ev.wtrans_, *ev.wbuf_, false /*No fin write*/);

    if (!r) // Flush the data to the disk to free meta/data space
        sm_->enqueue_defr_event(awsm::ev_do_async_flush{});

    *ev.res_ = r;
}

void agg_writer::do_fin_write(awsm::ev_do_fin_write& ev) noexcept
{
    // The write buffer could be empty, if the write from the upper layer gets
    // interrupted before being started. In addition, we doesn't support
    // fragments smaller than the min allowed size. In such cases we only need
    // to add the write_transaction for later finishing.
    if ((ev.wbuf_->size() < range_elem::min_rng_size()) ||
        do_write_impl(*ev.wtrans_, *ev.wbuf_, true /*fin write*/))
    {
        // Note that even if this is the final write for a given transaction
        // it may not lead to a finished transaction because the connection
        // may have been closed prematurely.
        finished_trans_.push_back(std::move(*ev.wtrans_));
        *ev.wbuf_ = frag_write_buff{};
    }
    else
    {
        // Set the pending data to be written first after the flush
        auto& pd = sdata_->pend_data_;
        X3ME_ASSERT(pd.buff_.empty() && !pd.trans_.valid(),
                    "Invalid pending data state");
        pd.buff_  = std::move(*ev.wbuf_);
        pd.trans_ = std::move(*ev.wtrans_);

        // Flush the data to the disk to free meta/data space
        sm_->enqueue_defr_event(awsm::ev_do_async_flush{});
    }
}

bool agg_writer::do_write_impl(write_transaction& wtrans,
                               const frag_write_buff& wbuf,
                               bool fin_write) noexcept
{
    X3ME_ASSERT(wbuf.size() <= wtrans.remaining_bytes(),
                "Buffer doesn't correspond to the transaction");

    const auto& key = wtrans.fs_node_key();
    const range rng{wtrans.curr_offset(), wbuf.size(), frag_rng};
    const agg_write_block::frag_ro_buff_t frag{wbuf};

    const auto res = fs_ops_->fsmd_add_new_fragment(
        key, rng, frag, sdata_->write_pos_, write_block_);
    XLOG_DEBUG(disk_tag, "Do_write agg_writer {}. Written frag. Fin_write {}. "
                         "Trans {}. Rng {}. Wr_pos {}. Res {}",
               log_ptr(this), fin_write, key, rng, sdata_->wr_pos(), res);
    if (res)
        wtrans.inc_written(rng.len());

    return res;
}

////////////////////////////////////////////////////////////////////////////////

void agg_writer::begin_flush() noexcept
{
    stats_fs_wr sts;
    const auto block = write_block_->begin_disk_write(sts);

    inc_stat(stats_.written_meta_size_, sts.written_meta_size_);
    inc_stat(stats_.wasted_meta_size_, sts.wasted_meta_size_);
    inc_stat(stats_.written_data_size_, sts.written_data_size_);
    inc_stat(stats_.wasted_data_size_, sts.wasted_data_size_);

    // Yeah, that cast is ugly. It's due to the lack of const correctness
    // in the aio_data structure. The operation is really read-only for the
    // fragment buffer. It's just that the aio_data behavior is not correct.
    aio_data_.buf_  = const_cast<uint8_t*>(block.data());
    aio_data_.size_ = block.size();
    aio_data_.offs_ = sdata_->wr_pos();

    XLOG_DEBUG(disk_tag,
               "Begin_flush agg_writer {}. Pos {} bytes. Size {} bytes",
               log_ptr(this), aio_data_.offs_, block.size());
}

void agg_writer::on_flush_done(const awsm::ev_io_done& ev) noexcept
{
    X3ME_ASSERT((aio_data_.offs_ == sdata_->wr_pos()), "Wrong state logic");
    if (*ev.err_)
    {
        XLOG_FATAL(disk_tag, "On_flush_done agg_writer {}. FS '{}'. Disk error "
                             "while flushing the aggregate buffer. Wr_pos {}. "
                             "Size {}. {}",
                   log_ptr(this), fs_ops_->vol_path(), aio_data_.offs_,
                   aio_data_.size_, ev.err_->message());
        fs_ops_->report_disk_error();
    }
    else
    {
        XLOG_DEBUG(disk_tag, "On_flush_done agg_writer {}. Wr_pos {}. Size {}",
                   log_ptr(this), fs_ops_->vol_path(), aio_data_.offs_,
                   aio_data_.size_);
    }
    // TODO We need a safe mechanic to remove the corresponding entries
    // from the metadata in case of disk error. The problems with removing
    // is due to the fact that some of the entries could have readers.
    // Due to the missing mechanic we do the wrong thing here and
    // commit non written entries which could lead to corrupted data being
    // read later.
    const auto wpos_info = fs_ops_->fsmd_commit_disk_write(
        sdata_->write_pos_, finished_trans_, write_block_);
    finished_trans_.clear();
    sdata_->write_pos_.set_from_bytes(wpos_info.write_pos_);
    sdata_->is_first_lap_ = (wpos_info.write_lap_ == 0);
}

////////////////////////////////////////////////////////////////////////////////

void agg_writer::do_last_flush() noexcept
{
    XLOG_INFO(disk_tag, "Do_last_flush agg_writer {}. FS '{}'", log_ptr(this),
              fs_ops_->vol_path());
    // All AIO threads must have been stopped before the call to this
    // function. We'll play safe though and use appropriate locks,
    // even if they are not really needed.
    fs_ops_->vmtx_wait_disk_readers();

    fs_ops_->fsmd_fin_flush_commit(sdata_->write_pos_, finished_trans_,
                                   write_block_);
}

} // namespace detail
} // namespace cache
