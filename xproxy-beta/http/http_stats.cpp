#include "precompiled.h"
#include "http_stats.h"

namespace http
{

var_stats& var_stats::operator+=(const var_stats& rhs) noexcept
{
    cnt_all_trans_ += rhs.cnt_all_trans_;
    cnt_all_trans_hit_ += rhs.cnt_all_trans_hit_;

    cnt_all_cacheable_trans_ += rhs.cnt_all_cacheable_trans_;
    cnt_all_http_tunnel_trans_ += rhs.cnt_all_http_tunnel_trans_;
    cnt_all_unsupported_req_ += rhs.cnt_all_unsupported_req_;
    cnt_all_error_req_ += rhs.cnt_all_error_req_;
    cnt_all_unsupported_resp_ += rhs.cnt_all_unsupported_resp_;
    cnt_all_error_resp_ += rhs.cnt_all_error_resp_;

    cnt_server_talks_first_ += rhs.cnt_server_talks_first_;
    cnt_server_talks_early_ += rhs.cnt_server_talks_early_;

    cnt_all_req_ += rhs.cnt_all_req_;
    cnt_all_resp_200_ += rhs.cnt_all_resp_200_;
    cnt_all_resp_206_ += rhs.cnt_all_resp_206_;
    cnt_all_resp_other_ += rhs.cnt_all_resp_other_;

    size_all_req_ += rhs.size_all_req_;
    size_all_resp_200_ += rhs.size_all_resp_200_;
    size_all_resp_206_ += rhs.size_all_resp_206_;
    size_all_resp_other_ += rhs.size_all_resp_other_;

    cnt_ccompare_skip_ += rhs.cnt_ccompare_skip_;
    cnt_ccompare_ok_ += rhs.cnt_ccompare_ok_;
    cnt_ccompare_fail_ += rhs.cnt_ccompare_fail_;
    bytes_ccompare_ += rhs.bytes_ccompare_;

    cnt_bpctrl_entries_ += rhs.cnt_bpctrl_entries_;

    return *this;
}

////////////////////////////////////////////////////////////////////////////////

constexpr resp_size_stats::hdr_lims_arr_t resp_size_stats::hdr_lims;
constexpr resp_size_stats::all_lims_arr_t resp_size_stats::perc_all_lims;

resp_size_stats::resp_size_stats() noexcept : data_(std::make_unique<data_t>())
{
}

resp_size_stats::~resp_size_stats() noexcept
{
}

resp_size_stats::resp_size_stats(const resp_size_stats& rhs) noexcept
    : data_(std::make_unique<data_t>(*rhs.data_))
{
}

resp_size_stats& resp_size_stats::operator=(const resp_size_stats& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        *data_ = *rhs.data_;
    }
    return *this;
}

void resp_size_stats::record_stats(bytes32_t hdr_len,
                                   bytes64_t all_len) noexcept
{
    const auto idx = find_stats_idx(hdr_len, all_len);
    auto& entry    = (*data_)[idx.first][idx.second];
    entry.count_ += 1;
    entry.bytes_ += all_len;
}

resp_size_stats& resp_size_stats::
operator+=(const resp_size_stats& rhs) noexcept
{
    auto& cd       = (*data_);
    const auto& rd = (*rhs.data_);
    for (size_t i = 0; i < rd.size(); ++i)
    {
        auto& ce       = cd[i];
        const auto& re = rd[i];
        for (size_t j = 0; j < re.size(); ++j)
            ce[j] += re[j];
    }
    return *this;
}

std::pair<uint32_t, uint32_t>
resp_size_stats::find_stats_idx(bytes32_t hdr_len, bytes64_t all_len) noexcept
{
    auto it = std::find_if(hdr_lims.begin(), hdr_lims.end(), [hdr_len](auto lim)
                           {
                               return hdr_len <= lim;
                           });
    // Check the actual header length if we are out of the checked bounds.
    const auto chdr_len = (it != hdr_lims.end()) ? *it : hdr_len;
    auto it2 = std::find_if(perc_all_lims.begin(), perc_all_lims.end(),
                            [chdr_len, all_len](auto perc)
                            {
                                return all_len <= all_len_lim(chdr_len, perc);
                            });
    const uint32_t hdr_len_idx = it - hdr_lims.begin();
    const uint32_t all_len_idx = it2 - perc_all_lims.begin();
    return std::make_pair(hdr_len_idx, all_len_idx);
}

} // namespace http
