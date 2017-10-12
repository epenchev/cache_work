#pragma once

namespace http
{
namespace detail
{

// Returns unix timestamp if parsing is successful
optional_t<time_t> parse_http_date(const char* http_date) noexcept;

} // namespace detail
} // namespace http
