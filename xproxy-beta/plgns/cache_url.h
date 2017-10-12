#pragma once

namespace xutils
{
class pcrex;
} // namespace xutils
namespace plgns
{
namespace detail
{
struct pattern;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

class cache_url
{
    std::vector<detail::pattern> patterns_;

public:
    cache_url() noexcept;
    ~cache_url() noexcept;

    cache_url(const cache_url&) = delete;
    cache_url& operator=(const cache_url&) = delete;
    cache_url(cache_url&&) = delete;
    cache_url& operator=(cache_url&&) = delete;

    void init(std::istream& cfg_data);

    void produce_cache_url(const string_view_t& orig_url,
                           boost_string_t& cache_url) noexcept;

private:
    template <typename Fun>
    void match_pattern(const string_view_t& orig_url, Fun&& fun) const noexcept;

    static std::pair<boost_string_t, boost_string_t>
    split_pattern_replace(const boost_string_t& line);
    static xutils::pcrex produce_regex(const boost_string_t& pattern);
    template <typename Container>
    void get_replace_offsets(const boost_string_t& line,
                             boost_string_t& replace,
                             Container& offsets);
};

} // namespace plgns
