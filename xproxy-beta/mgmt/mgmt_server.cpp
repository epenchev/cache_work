#include "precompiled.h"
#include "mgmt_server.h"
#include "cache/cache_stats.h"
#include "http/http_stats.h"
#include "net/net_stats.h"
#include "xutils/moveable_handler.h"

using namespace x3me::net;
using namespace x3me::net::json_rpc;

namespace
{

template <typename T>
void add_to_obj(json_rpc::document_t& d,
                json_rpc::string_ref_t name,
                T&& val) noexcept
{
    d.AddMember(name, val, d.GetAllocator());
}

template <typename T>
void add_to_obj(json_rpc::document_t& d,
                json_rpc::value_t& v,
                json_rpc::string_ref_t name,
                T&& val) noexcept
{
    v.AddMember(name, val, d.GetAllocator());
}

template <typename T, size_t Size>
void add_to_obj(json_rpc::document_t& d,
                const x3me::str_utils::stack_string<Size>& name,
                T&& val) noexcept
{
    json_rpc::value_t name_val(name.data(), name.size(), d.GetAllocator());
    d.AddMember(name_val, val, d.GetAllocator());
}

template <typename T, size_t Size>
void add_to_obj(json_rpc::document_t& d,
                json_rpc::value_t& v,
                const x3me::str_utils::stack_string<Size>& name,
                T&& val) noexcept
{
    json_rpc::value_t name_val(name.data(), name.size(), d.GetAllocator());
    v.AddMember(name_val, val, d.GetAllocator());
}

////////////////////////////////////////////////////////////////////////////////

template <typename N1, typename N2>
inline auto div_non_null(N1 num1, N2 num2) noexcept
{
    return (num2 != N2{}) ? num1 / num2 : num1;
}

// Rounds to 3 digits after the point
inline double round3(double v)
{
    return std::round(v * 1000.0) / 1000.0;
}

inline double bytes_to_kbytes(uint64_t bytes) noexcept
{
    return round3(bytes / 1024.0);
}

inline double bytes_to_mbytes(uint64_t bytes) noexcept
{
    return round3(bytes / (1024.0 * 1024.0));
}

inline double bytes_to_gbytes(uint64_t bytes) noexcept
{
    return round3(bytes / (1024.0 * 1024.0 * 1024.0));
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

namespace mgmt
{

mgmt_server::mgmt_server(const settings& sts) noexcept : impl_(ios_),
                                                         settings_(sts)
{
}

mgmt_server::~mgmt_server() noexcept
{
    X3ME_ENFORCE(!runner_.joinable()); // Stop must have been called
}

bool mgmt_server::start(const ip_addr4_t& bind_ip, uint16_t bind_port) noexcept
{
    err_code_t err;
    if (!impl_.start(bind_ip, bind_port, err))
    {
        XLOG_ERROR(main_tag,
                   "Unable to start the embedded JSON-RPC server on {}:{}. {}",
                   bind_ip, bind_port, err.message());
        return false;
    }

    subscribe();

    runner_ =
        std::thread([this]
                    {
                        x3me::sys_utils::set_this_thread_name("xproxy_mgmt");
                        ios_.run(); // May throw but, better crash in this case
                    });

    return true;
}

void mgmt_server::stop() noexcept
{
    impl_.stop();
    ios_.stop();
    if (runner_.joinable())
        runner_.join();
}

void mgmt_server::subscribe() noexcept
{
    impl_.add_callback(
        "debug_on",
        [this](json_rpc::json_rpc_res& ret, const json_rpc::string_ref_t& cmd)
        {
            std::string err;
            const auto& sts = settings_;
            if (start_rt_debug(sts, string_view_t(cmd.s, cmd.length), err))
                respond_debug_cmd(std::move(ret), "OK");
            else
                respond_debug_cmd(std::move(ret), err);
        });
    impl_.add_callback(
        "debug_off",
        [this](json_rpc::json_rpc_res& ret, const json_rpc::string_ref_t& cmd)
        {
            std::string err;
            if (stop_rt_debug(string_view_t(cmd.s, cmd.length), err))
                respond_debug_cmd(std::move(ret), "OK");
            else
                respond_debug_cmd(std::move(ret), err);
        });
    impl_.add_callback("summary_net_stats", [this](json_rpc::json_rpc_res& ret)
                       {
                           fn_summary_net_stats(
                               make_cb(ret, &respond_summary_net_stats));
                       });
    impl_.add_callback("summary_http_stats", [this](json_rpc::json_rpc_res& ret)
                       {
                           fn_summary_http_stats(
                               make_cb(ret, &respond_summary_http_stats));
                       });
    impl_.add_callback("resp_size_http_stats",
                       [this](json_rpc::json_rpc_res& ret)
                       {
                           fn_resp_size_http_stats(
                               make_cb(ret, &respond_resp_size_http_stats));
                       });
    impl_.add_callback(
        "summary_cache_stats", [this](json_rpc::json_rpc_res& ret)
        {
            fn_cache_stats(make_cb(ret, &respond_summary_cache_stats));
        });
    impl_.add_callback(
        "detailed_cache_stats", [this](json_rpc::json_rpc_res& ret)
        {
            fn_cache_stats(make_cb(ret, &respond_detailed_cache_stats));
        });
    impl_.add_callback("summary_internal_cache_stats",
                       [this](json_rpc::json_rpc_res& ret)
                       {
                           fn_cache_internal_stats(make_cb(
                               ret, &respond_summary_internal_cache_stats));
                       });
}

////////////////////////////////////////////////////////////////////////////////

void mgmt_server::respond_debug_cmd(json_rpc_res&& res,
                                    string_view_t ret) noexcept
{
    json_rpc::document_t val;
    val.SetString(ret.data(), ret.size(), val.GetAllocator());
    res.write_response(std::move(val));
}

void mgmt_server::respond_summary_net_stats(json_rpc_res&& res,
                                            net::all_stats&& as) noexcept
{
    const auto conn_failed =
        div_non_null(double(as.cnt_connect_failed_),
                     as.cnt_connect_success_ + as.cnt_connect_failed_);

    json_rpc::document_t val;
    val.SetObject();

    add_to_obj(val, "CurrConns", as.cnt_curr_conns_);
    add_to_obj(val, "CurrConecting", as.cnt_curr_connecting_);
    add_to_obj(val, "CurrBlindTunnel", as.cnt_curr_blind_tunnel_);
    add_to_obj(val, "BytesClientRecv", as.bytes_all_client_recv_);
    add_to_obj(val, "BytesOriginRecv", as.bytes_all_origin_recv_);
    add_to_obj(val, "BytesOriginSend", as.bytes_all_origin_send_);
    add_to_obj(val, "BytesClientSend", as.bytes_all_client_send_);
    add_to_obj(val, "BytesClientSendHIT", as.bytes_hit_client_send_);
    add_to_obj(val, "CntHalfClosed", as.cnt_half_closed_);
    add_to_obj(val, "CntHalfClosedClnRecv", as.cnt_half_closed_cln_recv_);
    add_to_obj(val, "CntHalfClosedOrgRecv", as.cnt_half_closed_org_recv_);
    add_to_obj(val, "CntHalfClosedClnClosed", as.cnt_half_closed_cln_closed_);
    add_to_obj(val, "CntHalfClosedOrgClosed", as.cnt_half_closed_org_closed_);
    add_to_obj(val, "ClosedHalfClosed", as.cnt_closed_half_closed_);
    add_to_obj(val, "CurrHalfClosed", as.cnt_curr_half_closed_);
    add_to_obj(val, "ConnectFailed_%", round3(conn_failed));
    add_to_obj(val, "ConnectFailed", as.cnt_connect_failed_);

    res.write_response(std::move(val));
}

void mgmt_server::respond_summary_http_stats(json_rpc_res&& res,
                                             http::var_stats&& vs) noexcept
{
    const bytes32_t avg_size_req =
        div_non_null(vs.size_all_req_, vs.cnt_all_req_);
    const bytes32_t avg_size_resp_200 =
        div_non_null(vs.size_all_resp_200_, vs.cnt_all_resp_200_);
    const bytes32_t avg_size_resp_206 =
        div_non_null(vs.size_all_resp_206_, vs.cnt_all_resp_206_);
    const bytes32_t avg_size_resp_other =
        div_non_null(vs.size_all_resp_other_, vs.cnt_all_resp_other_);
    const bytes32_t avg_size_ccompare = div_non_null(
        vs.bytes_ccompare_, vs.cnt_ccompare_ok_ + vs.cnt_ccompare_fail_);

    json_rpc::document_t val;
    val.SetObject();

    add_to_obj(val, "AllStartedTrans", vs.cnt_all_trans_);
    add_to_obj(val, "AllStartedTransHIT", vs.cnt_all_trans_hit_);
    add_to_obj(val, "CacheableTrans", vs.cnt_all_cacheable_trans_);
    add_to_obj(val, "HttpTunnelTrans", vs.cnt_all_http_tunnel_trans_);
    add_to_obj(val, "UnsupportedReqs", vs.cnt_all_unsupported_req_);
    add_to_obj(val, "ErrorReqs", vs.cnt_all_error_req_);
    add_to_obj(val, "UnsupportedResps", vs.cnt_all_unsupported_resp_);
    add_to_obj(val, "ErrorResps", vs.cnt_all_error_resp_);

    add_to_obj(val, "ServerTalksFirst", vs.cnt_server_talks_first_);
    add_to_obj(val, "ServerTalksEarly", vs.cnt_server_talks_early_);

    add_to_obj(val, "CntAllReq", vs.cnt_all_req_);
    add_to_obj(val, "CntAllResp200", vs.cnt_all_resp_200_);
    add_to_obj(val, "CntAllResp206", vs.cnt_all_resp_206_);
    add_to_obj(val, "CntAllRespOther", vs.cnt_all_resp_other_);
    add_to_obj(val, "BytesAllReq", vs.size_all_req_);
    add_to_obj(val, "BytesAllResp200", vs.size_all_resp_200_);
    add_to_obj(val, "BytesAllResp206", vs.size_all_resp_206_);
    add_to_obj(val, "BytesAllRespOther", vs.size_all_resp_other_);

    add_to_obj(val, "AvgSizeReq", avg_size_req);
    add_to_obj(val, "AvgSizeResp200", avg_size_resp_200);
    add_to_obj(val, "AvgSizeResp206", avg_size_resp_206);
    add_to_obj(val, "AvgSizeRespOther", avg_size_resp_other);

    add_to_obj(val, "CntCCompareSkip", vs.cnt_ccompare_skip_);
    add_to_obj(val, "CntCCompareOK", vs.cnt_ccompare_ok_);
    add_to_obj(val, "CntCCompareFail", vs.cnt_ccompare_fail_);
    add_to_obj(val, "AvgSizeCCompare", avg_size_ccompare);

    add_to_obj(val, "BPCTRL_Entries", vs.cnt_bpctrl_entries_);

    res.write_response(std::move(val));
}

void mgmt_server::respond_resp_size_http_stats(
    jr_res&& res, http::resp_size_stats&& st) noexcept
{
    auto hdr_name = [](uint32_t idx)
    {
        char buff[24];
        size_t len; // snprintf returns int, we want to convert it intentionally
        if (idx < http::resp_size_stats::hdr_lims.size())
        {
            const auto hlen = http::resp_size_stats::hdr_lims[idx];
            len = ::snprintf(buff, sizeof(buff), "HLen_LTE_%.1fKB",
                             bytes_to_kbytes(hlen));
        }
        else
        {
            const auto hlen = http::resp_size_stats::hdr_lims.back();
            len = ::snprintf(buff, sizeof(buff), "HLen_GE_%.1fKB",
                             bytes_to_kbytes(hlen));
        }
        len = std::min(len, sizeof(buff));
        return x3me::str_utils::stack_string<sizeof(buff)>(buff, len);
    };
    auto len_name = [](uint32_t idx_hlen, uint32_t idx_alen)
    {
        char buff[24];
        size_t len; // snprintf returns int, we want to convert it intentionally
        if ((idx_hlen < http::resp_size_stats::hdr_lims.size()) &&
            (idx_alen < http::resp_size_stats::perc_all_lims.size()))
        {
            const auto hlen = http::resp_size_stats::hdr_lims[idx_hlen];
            const auto perc = http::resp_size_stats::perc_all_lims[idx_alen];
            const auto alen = http::resp_size_stats::all_len_lim(hlen, perc);

            len = ::snprintf(buff, sizeof(buff), "LTE_%.2fKB_%u%%",
                             bytes_to_kbytes(alen), perc);
        }
        else if (idx_alen < http::resp_size_stats::perc_all_lims.size())
        {
            auto perc = http::resp_size_stats::perc_all_lims[idx_alen];
            len       = ::snprintf(buff, sizeof(buff), "LTE_%u%%", perc);
        }
        else
        {
            auto perc = http::resp_size_stats::perc_all_lims.back();
            len       = ::snprintf(buff, sizeof(buff), "GE_%u%%", perc);
        }
        len = std::min(len, sizeof(buff));
        return x3me::str_utils::stack_string<sizeof(buff)>(buff, len);
    };

    json_rpc::document_t val;
    val.SetObject();

    for (uint32_t i = 0; i < http::resp_size_stats::cnt_lims_hdr_len; ++i)
    {
        json_rpc::value_t tmp;
        tmp.SetObject();
        for (uint32_t j = 0; j < http::resp_size_stats::cnt_lims_all_len; ++j)
        {
            const auto cnt = st.get_counter(i, j);

            json_rpc::value_t arr;
            arr.SetArray();
            arr.PushBack(cnt.count_, val.GetAllocator());
            arr.PushBack(bytes_to_mbytes(cnt.bytes_), val.GetAllocator());

            add_to_obj(val, tmp, len_name(i, j), std::move(arr));
        }
        add_to_obj(val, hdr_name(i), std::move(tmp));
    }

    res.write_response(std::move(val));
}

void mgmt_server::respond_summary_cache_stats(
    json_rpc_res&& res, std::vector<cache::stats_fs>&& st) noexcept
{
    struct sum_stats
    {
        bytes64_t bytes_total_        = 0;
        bytes64_t bytes_used_         = 0;
        bytes64_t bytes_entries_data_ = 0;
        uint64_t cnt_entries_         = 0;
        uint64_t cnt_objects_         = 0;

        bytes64_t written_meta_size_ = 0;
        bytes64_t wasted_meta_size_  = 0;
        bytes64_t written_data_size_ = 0;
        bytes64_t wasted_data_size_  = 0;

        uint64_t cnt_block_meta_read_ok_   = 0;
        uint64_t cnt_block_meta_read_err_  = 0;
        uint64_t cnt_evac_entries_checked_ = 0;
        uint64_t cnt_evac_entries_todo_    = 0;
        uint64_t cnt_evac_entries_ok_      = 0;
        uint64_t cnt_evac_entries_err_     = 0;

        bytes64_t min_written_data_size_ = static_cast<bytes64_t>(-1);
        bytes64_t max_written_data_size_ = 0;

        uint32_t cnt_pending_reads_  = 0;
        uint32_t cnt_pending_writes_ = 0;

        uint16_t cnt_errors_ = 0;
    } ss;
    // Accumulate the stats
    for (const auto& s : st)
    {
        ss.bytes_total_ += s.data_end_;
        ss.bytes_used_ += (s.write_lap_ > 0 ? s.data_end_ : s.write_pos_);
        ss.bytes_entries_data_ += s.entries_data_size_;
        ss.cnt_entries_ += s.cnt_entries_;
        ss.cnt_objects_ += s.cnt_nodes_;

        ss.written_meta_size_ += s.written_meta_size_;
        ss.wasted_meta_size_ += s.wasted_meta_size_;
        ss.written_data_size_ += s.written_data_size_;
        ss.wasted_data_size_ += s.wasted_data_size_;

        ss.cnt_block_meta_read_ok_ += s.cnt_block_meta_read_ok_;
        ss.cnt_block_meta_read_err_ += s.cnt_block_meta_read_err_;
        ss.cnt_evac_entries_checked_ += s.cnt_evac_entries_checked_;
        ss.cnt_evac_entries_todo_ += s.cnt_evac_entries_todo_;
        ss.cnt_evac_entries_ok_ += s.cnt_evac_entries_ok_;
        ss.cnt_evac_entries_err_ += s.cnt_evac_entries_err_;

        if (ss.min_written_data_size_ > s.written_data_size_)
            ss.min_written_data_size_ = s.written_data_size_;
        if (ss.max_written_data_size_ < s.written_data_size_)
            ss.max_written_data_size_ = s.written_data_size_;

        ss.cnt_pending_reads_ += s.cnt_pending_reads_;
        ss.cnt_pending_writes_ += s.cnt_pending_writes_;

        ss.cnt_errors_ += s.cnt_errors_;
    }
    const uint16_t cnt_volumes = st.size();
    const auto wasted_meta_size_pr =
        div_non_null(double(ss.wasted_meta_size_), ss.written_meta_size_) *
        100.0;
    const auto wasted_data_size_pr =
        div_non_null(double(ss.wasted_data_size_), ss.written_data_size_) *
        100.0;
    const auto block_meta_read_ok_pr =
        div_non_null(double(ss.cnt_block_meta_read_ok_),
                     ss.cnt_block_meta_read_ok_ + ss.cnt_block_meta_read_err_) *
        100.0;
    const auto evac_entries_pr = div_non_null(double(ss.cnt_evac_entries_todo_),
                                              ss.cnt_evac_entries_checked_) *
                                 100.0;
    const auto evac_entries_read_err_pr =
        div_non_null(double(ss.cnt_evac_entries_err_),
                     ss.cnt_evac_entries_ok_ + ss.cnt_evac_entries_err_) *
        100.0;
    const bytes64_t avg_written_data_size =
        div_non_null(ss.written_data_size_, cnt_volumes);

    json_rpc::document_t val;
    val.SetObject();

    add_to_obj(val, "CntVolumes", cnt_volumes);
    add_to_obj(val, "CntErrors", ss.cnt_errors_);
    add_to_obj(val, "BytesTotal", ss.bytes_total_);
    add_to_obj(val, "BytesUsed", ss.bytes_used_);
    add_to_obj(val, "BytesEntriesData", ss.bytes_entries_data_);
    add_to_obj(val, "CntEntries", ss.cnt_entries_);
    add_to_obj(val, "CntObjects", ss.cnt_objects_);
    add_to_obj(val, "PendingReads", ss.cnt_pending_reads_);
    add_to_obj(val, "PendingWrites", ss.cnt_pending_writes_);
    add_to_obj(val, "AllWrittenBlockMeta_MB",
               bytes_to_mbytes(ss.written_meta_size_));
    add_to_obj(val, "AllWrittenData_GB",
               bytes_to_gbytes(ss.written_data_size_));
    add_to_obj(val, "AvgWrittenData_MB",
               bytes_to_mbytes(avg_written_data_size));
    add_to_obj(val, "MinWrittenData_MB",
               bytes_to_mbytes(ss.min_written_data_size_));
    add_to_obj(val, "MaxWrittenData_MB",
               bytes_to_mbytes(ss.max_written_data_size_));
    add_to_obj(val, "WastedBlockMetaSpace_Pr", round3(wasted_meta_size_pr));
    add_to_obj(val, "WastedBlockDataSpace_Pr", round3(wasted_data_size_pr));
    add_to_obj(val, "BlockMetaReadOk_Pr", round3(block_meta_read_ok_pr));
    add_to_obj(val, "EvacEntries_Pr", round3(evac_entries_pr));
    add_to_obj(val, "EvacEntriesReadErr_Pr", round3(evac_entries_read_err_pr));
    add_to_obj(val, "CheckedEvacEntries", ss.cnt_evac_entries_checked_);

    res.write_response(std::move(val));
}

void mgmt_server::respond_detailed_cache_stats(
    json_rpc_res&& res, std::vector<cache::stats_fs>&& st) noexcept
{
    json_rpc::document_t val;
    val.SetObject();

    for (const auto& s : st)
    {
        const auto wasted_meta_size_pr =
            div_non_null(double(s.wasted_meta_size_), s.written_meta_size_) *
            100.0;
        const auto wasted_data_size_pr =
            div_non_null(double(s.wasted_data_size_), s.written_data_size_) *
            100.0;
        const auto block_meta_read_ok_pr =
            div_non_null(double(s.cnt_block_meta_read_ok_),
                         s.cnt_block_meta_read_ok_ +
                             s.cnt_block_meta_read_err_) *
            100.0;
        const auto evac_entries_pr =
            div_non_null(double(s.cnt_evac_entries_todo_),
                         s.cnt_evac_entries_checked_) *
            100.0;
        const auto evac_entries_read_err_pr =
            div_non_null(double(s.cnt_evac_entries_err_),
                         s.cnt_evac_entries_ok_ + s.cnt_evac_entries_err_) *
            100.0;
        const bytes32_t avg_entry_size =
            div_non_null(s.entries_data_size_, s.cnt_entries_);
        const bytes32_t avg_object_size =
            div_non_null(s.entries_data_size_, s.cnt_nodes_);

        json_rpc::value_t tmp;
        tmp.SetObject();

        add_to_obj(val, "CntEntries", s.cnt_entries_);
        add_to_obj(val, "CntObjects", s.cnt_nodes_);
        add_to_obj(val, "AvgEntrySize", avg_entry_size);
        add_to_obj(val, "AvgObjectSize", avg_object_size);

        add_to_obj(val, tmp, "PendingReads", s.cnt_pending_reads_);
        add_to_obj(val, tmp, "PendingWrites", s.cnt_pending_writes_);

        add_to_obj(val, tmp, "WrittenBlockMeta_MB",
                   bytes_to_mbytes(s.written_meta_size_));
        add_to_obj(val, tmp, "WrittenData_GB",
                   bytes_to_gbytes(s.written_data_size_));

        add_to_obj(val, tmp, "WastedBlockMetaSpace_Pr",
                   round3(wasted_meta_size_pr));
        add_to_obj(val, tmp, "WastedDataSpace_Pr", round3(wasted_data_size_pr));

        add_to_obj(val, "BlockMetaReadOk_Pr", round3(block_meta_read_ok_pr));
        add_to_obj(val, "EvacEntries_Pr", round3(evac_entries_pr));
        add_to_obj(val, "EvacEntriesReadErr_Pr",
                   round3(evac_entries_read_err_pr));
        add_to_obj(val, "CheckedEvacEntries", s.cnt_evac_entries_checked_);

        add_to_obj(val, tmp, "FsMetaNodes", s.cnt_nodes_);
        add_to_obj(val, tmp, "FsMetaRanges", s.cnt_ranges_);
        add_to_obj(val, tmp, "FsTableSize_MB",
                   bytes_to_mbytes(s.curr_data_size_));
        add_to_obj(val, tmp, "FsTableAllowedSize_MB",
                   bytes_to_mbytes(s.max_allowed_data_size_));

        add_to_obj(val, tmp, "DataBeg_MB", bytes_to_mbytes(s.data_begin_));
        add_to_obj(val, tmp, "DataEnd_MB", bytes_to_mbytes(s.data_end_));
        add_to_obj(val, tmp, "WritePos_MB", bytes_to_mbytes(s.write_pos_));
        add_to_obj(val, tmp, "WriteLap", s.write_lap_);

        add_to_obj(val, tmp, "DiskErrors", s.cnt_errors_);

        x3me::str_utils::stack_string<24> path{s.path_.data(), s.path_.size()};
        add_to_obj(val, path, std::move(tmp));
    }

    res.write_response(std::move(val));
}

void mgmt_server::respond_summary_internal_cache_stats(
    jr_res&& res, std::vector<cache::stats_internal>&& st) noexcept
{
    cache::stats_internal ss; // sum stats
    // Accumulate the stats
    for (const auto& s : st)
    {
        ss.cnt_lock_volume_mtx_ += s.cnt_lock_volume_mtx_;
        ss.cnt_no_lock_volume_mtx_ += s.cnt_no_lock_volume_mtx_;
        ss.cnt_begin_write_ok_ += s.cnt_begin_write_ok_;
        ss.cnt_begin_write_fail_ += s.cnt_begin_write_fail_;
        ss.cnt_begin_write_trunc_ok_ += s.cnt_begin_write_trunc_ok_;
        ss.cnt_begin_write_trunc_fail_ += s.cnt_begin_write_trunc_fail_;
        ss.cnt_read_frag_mem_hit_ += s.cnt_read_frag_mem_hit_;
        ss.cnt_read_frag_mem_miss_ += s.cnt_read_frag_mem_miss_;
        ss.cnt_frag_meta_add_ok_ += s.cnt_frag_meta_add_ok_;
        ss.cnt_frag_meta_add_skipped_ += s.cnt_frag_meta_add_skipped_;
        ss.cnt_frag_meta_add_limit_ += s.cnt_frag_meta_add_limit_;
        ss.cnt_frag_meta_add_overlaps_ += s.cnt_frag_meta_add_overlaps_;
        ss.cnt_readers_limit_reached_ += s.cnt_readers_limit_reached_;
        ss.cnt_failed_unmark_read_rng_ += s.cnt_failed_unmark_read_rng_;
        ss.cnt_invalid_rng_elem_ += s.cnt_invalid_rng_elem_;
        ss.cnt_evac_frag_no_mem_entry_ += s.cnt_evac_frag_no_mem_entry_;
    }

    const auto lock_volume_mtx_pr =
        div_non_null(double(ss.cnt_lock_volume_mtx_),
                     ss.cnt_lock_volume_mtx_ + ss.cnt_no_lock_volume_mtx_) *
        100.0;
    const auto begin_write_fail_pr =
        div_non_null(double(ss.cnt_begin_write_fail_),
                     ss.cnt_begin_write_ok_ + ss.cnt_begin_write_fail_) *
        100.0;
    const auto begin_write_trunc_fail_pr =
        div_non_null(double(ss.cnt_begin_write_trunc_fail_),
                     ss.cnt_begin_write_trunc_ok_ +
                         ss.cnt_begin_write_trunc_fail_) *
        100.0;
    const auto read_frag_mem_hit_pr =
        div_non_null(double(ss.cnt_read_frag_mem_hit_),
                     ss.cnt_read_frag_mem_hit_ + ss.cnt_read_frag_mem_miss_) *
        100;

    json_rpc::document_t val;
    val.SetObject();

    add_to_obj(val, "LockVolMutex_Pr", round3(lock_volume_mtx_pr));
    add_to_obj(val, "BeginWriteFail_Pr", round3(begin_write_fail_pr));
    add_to_obj(val, "BeginWriteTruncFail_Pr",
               round3(begin_write_trunc_fail_pr));
    add_to_obj(val, "ReadFragMemHit_Pr", round3(read_frag_mem_hit_pr));
    add_to_obj(val, "CntFragMetaAddOk", ss.cnt_frag_meta_add_ok_);
    add_to_obj(val, "CntFragMetaAddSkip", ss.cnt_frag_meta_add_skipped_);
    add_to_obj(val, "CntFragMetaAddLimit", ss.cnt_frag_meta_add_limit_);
    add_to_obj(val, "CntFragMetaAddOverlaps", ss.cnt_frag_meta_add_overlaps_);
    add_to_obj(val, "CntReadersLimitReached", ss.cnt_readers_limit_reached_);
    add_to_obj(val, "CntFailUnmarkReadRng", ss.cnt_failed_unmark_read_rng_);
    add_to_obj(val, "CntInvalidRngEleme", ss.cnt_invalid_rng_elem_);
    add_to_obj(val, "CntEvacFragNoEntry", ss.cnt_evac_frag_no_mem_entry_);

    res.write_response(std::move(val));
}

////////////////////////////////////////////////////////////////////////////////
// Our callbacks that we pass to the external callbacks given to us can get
// called in any of the application threads. We need to ensure that the
// final work will be done in our management thread. Thus we need to
// post/dispatch every callback to our thread. This function is used save
// repeatedly writing the lambda with post/dispatch and the other lambda.
template <typename... Args>
std::function<void(Args...)> mgmt_server::make_cb(const json_rpc_res& ret,
                                                  void (*fn)(json_rpc_res&&,
                                                             Args...)) noexcept
{
    return [this, fn, ret](Args&&... args)
    {
        using xutils::make_moveable_handler;
        // Can't forward variadic number of arguments into the lambda capture
        // without tuple.
        auto t = std::make_tuple(ret, std::forward<Args>(args)...);
        ios_.dispatch(make_moveable_handler([ fn, t = std::move(t) ]() mutable
                                            {
                                                using std::experimental::apply;
                                                apply(fn, std::move(t));
                                            }));
    };
}

} // namespace mgmt
