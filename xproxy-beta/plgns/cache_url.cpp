#include "precompiled.h"
#include "cache_url.h"
#include "xutils/pcrex.h"

namespace plgns
{
namespace detail
{

struct pattern
{
    static constexpr uint32_t max_cnt_tokens = 10;
    static constexpr int32_t invalid_offset  = -1;

    using token_offsets_t = std::array<int32_t, max_cnt_tokens>;

    xutils::pcrex regex_;
    boost_string_t replacement_;
    token_offsets_t repl_offs_;

    pattern(xutils::pcrex&& regex,
            boost_string_t&& replace,
            const token_offsets_t& repl_offs) noexcept
        : regex_(std::move(regex)),
          replacement_(std::move(replace)),
          repl_offs_(repl_offs)
    {
    }
};

struct print_offsets
{
    pattern::token_offsets_t& offs_;
};

std::ostream& operator<<(std::ostream& os, const print_offsets& rhs) noexcept
{
    os << '[';
    for (uint32_t i = 0; i < rhs.offs_.size(); ++i)
    {
        if (rhs.offs_[i] != detail::pattern::invalid_offset)
            os << i << '=' << rhs.offs_[i] << ',';
    }
    return os << ']';
}

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

cache_url::cache_url() noexcept
{
}

cache_url::~cache_url() noexcept
{
}

void cache_url::init(std::istream& cfg_data)
{
    auto rem_comment = [](boost_string_t& s)
    {
        const auto pos = s.find('#');
        if (pos != boost_string_t::npos)
            s.resize(pos);
    };

    boost_string_t pattern;
    std::vector<detail::pattern> patterns;
    for (boost_string_t line; boost::container::getline(cfg_data, line);)
    {
        rem_comment(line);

        boost::algorithm::trim(line);

        if (line.empty())
            continue;

        boost_string_t replace;
        std::tie(pattern, replace) = split_pattern_replace(line);

        auto regex = produce_regex(pattern);

        detail::pattern::token_offsets_t offs;
        get_replace_offsets(line, replace, offs);

        XLOG_DEBUG(plgn_tag, "Cache_url plugin. Add entry. Regex: {}. Replace: "
                             "{}. Repl_offsets: {}",
                   pattern, replace, detail::print_offsets{offs});

        patterns.emplace_back(std::move(regex), std::move(replace), offs);
    }
    // Commit the successfully loaded patterns
    patterns_ = std::move(patterns);
}

void cache_url::produce_cache_url(const string_view_t& orig_url,
                                  boost_string_t& cache_url) noexcept
{
    using xutils::pcrex;

    cache_url.clear();
    match_pattern(
        orig_url,
        [&](const detail::pattern& pattern, const pcrex::matches_t& matches)
        {
            cache_url      = pattern.replacement_;
            uint32_t shift = 0;
            for (uint32_t i = 0; i < pcrex::matches_cnt(matches); ++i)
            {
                const auto repl_offs = pattern.repl_offs_[i];
                if (repl_offs != detail::pattern::invalid_offset)
                {
                    const auto moff = pcrex::match_offset(matches, i);
                    // TODO Change this check back to assertion when
                    // the bug is found
                    if ((moff.beg_ >= 0) && (moff.end_ > moff.beg_) &&
                        (moff.end_ <= (pcrex::offset_t)orig_url.size()))
                    {
                        const auto mlen  = moff.end_ - moff.beg_;
                        const char* mtch = &orig_url[moff.beg_];
                        // The repl_offset has been checked for validity against
                        // the replacement_ string during the initialization.
                        cache_url.insert(repl_offs + shift, mtch, mlen);
                        shift += mlen;
                    }
                    else
                    {
                        XLOG_FATAL(plgn_tag, "Cache_url plugin. Invalid match "
                                             "offsets. Beg: {}. End: {}. URL: "
                                             "{}",
                                   moff.beg_, moff.end_, orig_url);
                        std::cerr
                            << "Cache_url plugin. Invalid match offsets. Beg: "
                            << moff.beg_ << ". End: " << moff.end_
                            << ". URL: " << orig_url << std::endl;
                    }
                }
            }
        });
}

////////////////////////////////////////////////////////////////////////////////

template <typename Fun>
void cache_url::match_pattern(const string_view_t& orig_url, Fun&& fun) const
    noexcept
{
    using xutils::pcrex;
    constexpr auto matches_size =
        pcrex::matches_size(detail::pattern::max_cnt_tokens);
    std::array<pcrex::offset_t, matches_size> matches;
    for (const auto& p : patterns_)
    {
        if (const auto ret = p.regex_.match(orig_url, matches))
        {
            const auto& m = ret.value();
            if (!m.empty())
                fun(p, m);
        }
        else
        {
            // TODO Add string representation of the error code when there is
            // time. There are 20-30 kinds of errors and don't want to spend
            // time now for this. Meantime, we can read about the errors from
            // 'man pcreapi', section 'Error return values from pcre_exec()'
            XLOG_ERROR(plgn_tag,
                       "Cache_url plugin. Match error. URL: {}. Err_code: {}",
                       orig_url, ret.error());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

std::pair<boost_string_t, boost_string_t>
cache_url::split_pattern_replace(const boost_string_t& line)
{
    // Don't want to use boost::split or stringstream for this.
    const char* spaces = " \t";
    const auto pos1 = line.find_first_of(spaces);
    if (pos1 == boost_string_t::npos)
    {
        throw std::invalid_argument(
            ("No space(s) between pattern and replacement in line: " + line)
                .c_str());
    }
    const auto pos2 = line.find_first_not_of(spaces, pos1 + 1);
    X3ME_ENFORCE(pos2 != boost_string_t::npos,
                 "There must be non space symbol "
                 "because the line must have been "
                 "already trimmed");
    if (line.find_first_of(spaces, pos2) != boost_string_t::npos)
    {
        throw std::invalid_argument(
            ("More than pattern and replacement in line: " + line).c_str());
    }
    return std::make_pair(line.substr(0, pos1), line.substr(pos2));
}

xutils::pcrex cache_url::produce_regex(const boost_string_t& pattern)
{
    xutils::pcrex regex(pattern.c_str());
    if (regex.capture_count() > (detail::pattern::max_cnt_tokens - 1))
    {
        throw std::invalid_argument(
            ("Too many captures, more than 9, in pattern: " + pattern).c_str());
    }
    return regex;
}

template <typename Container>
void cache_url::get_replace_offsets(const boost_string_t& line,
                                    boost_string_t& replace,
                                    Container& offsets)
{
    X3ME_ASSERT(offsets.size() == 10, "We need to support up to 10 offsets");
    std::fill(offsets.begin(), offsets.end(), detail::pattern::invalid_offset);

    uint32_t cnt_tokens = 0;
    for (auto offs = replace.find('$'); offs != boost_string_t::npos;
         offs = replace.find('$', offs))
    {
        if (++cnt_tokens > offsets.size())
        {
            throw std::invalid_argument(
                ("Too many replacement tokens in line: " + line).c_str());
        }
        if (!(offs < (replace.size() - 1)))
        {
            throw std::invalid_argument(
                ("Invalid token at the end of line: " + line).c_str());
        }
        const char tok = replace[offs + 1];
        if ((tok < '0') || (tok > '9'))
        {
            throw std::invalid_argument(
                ("Invalid non single digit token in line: " + line).c_str());
        }

        offsets[tok - '0'] = offs;
        replace.erase(offs, 2);
    }
    // We could shrink_to_fit the replace string here, but ... meh :)
}

} // namespace plgns
