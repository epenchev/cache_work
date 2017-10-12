#include "precompiled.h"
#include "pcrex.h"

namespace xutils
{

pcrex::pcrex(const char* pattern)
{
    const char* err_info = nullptr;
    int err_offs = 0;
    rex_.reset(::pcre_compile(pattern, 0, &err_info, &err_offs, nullptr));
    if (!rex_)
    {
        x3me::utilities::string_builder_256 sb;
        sb << "Wrong regex: " << pattern << ". Error: " << err_info
           << ". Error position: " << err_offs;
        throw std::logic_error(sb.to_string());
    }
    // Trying to speed up the regex more.
    study_.reset(::pcre_study(rex_.get(), 0, &err_info));
    if (!study_)
    {
        x3me::utilities::string_builder_256 sb;
        sb << "Unable to study regex: " << pattern << ". Error: " << err_info;
        throw std::logic_error(sb.to_string());
    }
}

expected_t<pcrex::matches_t, pcrex::err_code_t>
pcrex::match(string_view_t str, matches_t matches) const noexcept
{
    const auto match_cnt =
        ::pcre_exec(rex_.get(), study_.get(), str.data(), str.size(), 0, 0,
                    matches.data(), matches.size());
    if (match_cnt == PCRE_ERROR_NOMATCH)
    {
        return matches_t{};
    }
    else if (match_cnt > 0)
    {
        X3ME_ASSERT(matches_size(match_cnt) <= matches.size());
        return matches_t{matches.data(), static_cast<uint32_t>(match_cnt) * 2};
    }
    // match_cnt equals 0 means that the matches.size() is not big enough.
    return boost::make_unexpected(err_code_t{match_cnt});
}

uint32_t pcrex::capture_count() const noexcept
{
    int cnt = 0;
    const int r =
        ::pcre_fullinfo(rex_.get(), study_.get(), PCRE_INFO_CAPTURECOUNT, &cnt);
    X3ME_ENFORCE(r == 0, "Wrong arguments passed to the 'pcre_full_info'");
    X3ME_ENFORCE(cnt >= 0, "The capture count must be at GTE 0");
    return static_cast<uint32_t>(cnt);
}

} // namespace xutils
