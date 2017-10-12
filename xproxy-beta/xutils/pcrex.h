#pragma once

namespace xutils
{

// A very simple wrapper around the Perl
// compatible regular expression functionality.
// Don't want to use the boost/std implementation because it does
// unneeded, for our usage, heap allocations to store the match results,
// even when regex_iterator functionality is used.
class pcrex
{
    struct del_pcre
    {
        void operator()(pcre* r) noexcept { ::pcre_free(r); }
    };
    struct del_pcre_extra
    {
        void operator()(pcre_extra* r) noexcept { ::pcre_free_study(r); }
    };
    using pcre_ptr_t       = std::unique_ptr<pcre, del_pcre>;
    using pcre_extra_ptr_t = std::unique_ptr<pcre_extra, del_pcre_extra>;

    pcre_ptr_t rex_;
    pcre_extra_ptr_t study_;

public:
    explicit pcrex(const char* pattern);

    pcrex(pcrex&&) noexcept = default;
    pcrex& operator=(pcrex&&) noexcept = default;
    ~pcrex() noexcept = default;

    pcrex(const pcrex&) = delete;
    pcrex& operator=(const pcrex&) = delete;

    using offset_t   = int;
    using matches_t  = x3me::mem_utils::array_view<offset_t>;
    using err_code_t = int;
    expected_t<matches_t, err_code_t> match(string_view_t str,
                                            matches_t matches) const noexcept;

    uint32_t capture_count() const noexcept;

    struct match_off
    {
        offset_t beg_;
        offset_t end_;
    };
    static match_off match_offset(const matches_t& matches,
                                  uint32_t idx) noexcept
    {
        return match_off{matches[idx * 2], matches[idx * 2 + 1]};
    }
    static uint32_t matches_cnt(const matches_t& matches) noexcept
    {
        return matches.size() / 2;
    }

    static constexpr uint32_t matches_size(uint32_t cnt_matches) noexcept
    {
        return cnt_matches * 3; // See 'man pcreapi' why this is in this way.
    }
};

} // namespace xutils
