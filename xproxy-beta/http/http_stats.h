#pragma once

namespace http
{

struct var_stats
{
    // Started transactions
    uint64_t cnt_all_trans_     = 0;
    uint64_t cnt_all_trans_hit_ = 0;
    // Finished/Completed transactions
    uint64_t cnt_all_cacheable_trans_   = 0;
    uint64_t cnt_all_http_tunnel_trans_ = 0;
    uint64_t cnt_all_unsupported_req_   = 0;
    uint64_t cnt_all_error_req_         = 0;
    uint64_t cnt_all_unsupported_resp_  = 0;
    uint64_t cnt_all_error_resp_        = 0;
    // The unfinished transactions will be the difference between
    // cnt_all_trans_ all the sum of the others.

    uint64_t cnt_server_talks_first_ = 0;
    uint64_t cnt_server_talks_early_ = 0;

    // The below counts are obtained only for the finished requests/responses.
    uint64_t cnt_all_req_        = 0;
    uint64_t cnt_all_resp_200_   = 0;
    uint64_t cnt_all_resp_206_   = 0;
    uint64_t cnt_all_resp_other_ = 0;

    // The below counts are obtained only for the finished requests/responses.
    bytes64_t size_all_req_        = 0;
    bytes64_t size_all_resp_200_   = 0;
    bytes64_t size_all_resp_206_   = 0;
    bytes64_t size_all_resp_other_ = 0;

    uint64_t cnt_ccompare_skip_ = 0;
    uint64_t cnt_ccompare_ok_   = 0;
    uint64_t cnt_ccompare_fail_ = 0;
    bytes64_t bytes_ccompare_   = 0;

    uint32_t cnt_bpctrl_entries_ = 0;

    var_stats& operator+=(const var_stats& rhs) noexcept;
};

////////////////////////////////////////////////////////////////////////////////

class resp_size_stats
{
public:
    static constexpr uint32_t cnt_lims_hdr_len = 17;
    static constexpr uint32_t cnt_lims_all_len = 6;

    using hdr_lims_arr_t = std::array<bytes32_t, cnt_lims_hdr_len - 1>;
    using all_lims_arr_t = std::array<uint32_t, cnt_lims_all_len - 1>;

    // clang-format off
    static constexpr hdr_lims_arr_t hdr_lims =
    {
        0.5_KB,
        1.0_KB,
        1.5_KB,
        2.0_KB,
        2.5_KB,
        3.0_KB,
        3.5_KB,
        4.0_KB,
        4.5_KB,
        5.0_KB,
        5.5_KB,
        6.0_KB,
        6.5_KB,
        7.0_KB,
        7.5_KB,
        8.0_KB
    };
    static constexpr all_lims_arr_t perc_all_lims = 
    {
        0,
        30,
        40,
        50,
        75
    };
    // clang-format on

    struct counter
    {
        uint64_t count_  = 0;
        bytes64_t bytes_ = 0;

        counter& operator+=(const counter& rhs) noexcept
        {
            count_ += rhs.count_;
            bytes_ += rhs.bytes_;
            return *this;
        }
    };

private:
    using dim2_t = std::array<counter, cnt_lims_all_len>;
    using data_t = std::array<dim2_t, cnt_lims_hdr_len>;

    std::unique_ptr<data_t> data_;

public:
    resp_size_stats() noexcept; // Terminate if the allocation fails
    ~resp_size_stats() noexcept;

    resp_size_stats(resp_size_stats&&) noexcept = default;
    resp_size_stats& operator=(resp_size_stats&&) noexcept = default;

    resp_size_stats(const resp_size_stats& rhs) noexcept;
    resp_size_stats& operator=(const resp_size_stats& rhs) noexcept;

    void record_stats(bytes32_t hdr_len, bytes64_t all_len) noexcept;

    resp_size_stats& operator+=(const resp_size_stats& rhs) noexcept;

    counter get_counter(uint32_t idx_hdr_len, uint32_t idx_all_len) noexcept
    {
        return (*data_)[idx_hdr_len][idx_all_len];
    }

    // Percent must be less than 100
    static bytes32_t all_len_lim(bytes32_t hdr_len, uint32_t perc) noexcept
    {
        return hdr_len * 100U / (100U - perc);
    }

private:
    static std::pair<uint32_t, uint32_t>
    find_stats_idx(bytes32_t hdr_len, bytes64_t all_len) noexcept;
};

////////////////////////////////////////////////////////////////////////////////

struct all_stats
{
    var_stats var_stats_;
    resp_size_stats resp_size_stats_;
};

} // namespace http
