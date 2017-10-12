#pragma once

namespace xutils
{

string_view_t get_host(const string_view_t& url) noexcept;

// Example: smth1.smth2.youtube.com
// domain_level = 2 => youtube.com
// domain_level = 3 => smth2.youtube.com
// domain_level = 4 => smth1.smth2.youtube.com
// domain_level = 0 => takes always all i.e. smth1.smth2.youtube.com
// xxx.xxx.xxx.xxx  => takes the whole dotted ip
string_view_t truncate_host(const string_view_t& host,
                            uint16_t domain_lvl) noexcept;

} // namespace xutils
