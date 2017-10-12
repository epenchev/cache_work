#include "precompiled.h"
#include "http_date.h"

namespace http
{
namespace detail
{

optional_t<time_t> parse_http_date(const char* http_date) noexcept
{
    constexpr auto formats = {
        "%a, %d %b %Y %H:%M:%S", // RFC 822, updated by RFC 1123
        "%A, %d-%b-%y %H:%M:%S", // RFC 850, obsoleted by RFC 1036
        "%a %b %d %H:%M:%S %Y", // ANSI C's asctime() format
        "%d %b %Y %H:%M:%S" // NNTP-style date
    };
    for (auto f : formats)
    {
        struct tm tmt = {};
        if (const auto p = ::strptime(http_date, f, &tmt))
        {
            // The matched date is allowed to ends with GMT/UTC/+0000 or
            // nothing. I have seen dates to end with any of the above strings.
            // Timezones are intentionally not supported like in the ATS.
            // However, we don't assume, like the ATS,
            // that everything is standard compliant and uses GMT time.
            if ((*p == 0) || (::strcmp(p, " GMT") == 0) ||
                (::strcmp(p, " UTC") == 0) || (::strcmp(p, " +0000") == 0))
            {
                return ::timegm(&tmt);
            }
        }
    }
    return optional_t<time_t>{};
}

} // namespace detail
} // namespace http
