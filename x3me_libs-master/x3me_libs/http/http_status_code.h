#pragma once

namespace x3me
{
namespace net
{

#define STATUS_CODES(MACRO)                                                    \
    MACRO(200, ok, "OK")                                                       \
    MACRO(201, created, "Created")                                             \
    MACRO(202, accepted, "Accepted")                                           \
    MACRO(204, no_content, "No Content")                                       \
    MACRO(206, partial_content, "Partial Content")                             \
    MACRO(300, multiple_choices, "Multiple Choices")                           \
    MACRO(301, moved_permanently, "Moved Permanently")                         \
    MACRO(302, moved_temporarily, "Moved Temporarily")                         \
    MACRO(304, not_modified, "Not Modified")                                   \
    MACRO(400, bad_request, "Bad Request")                                     \
    MACRO(401, unauthorized, "Unauthorized")                                   \
    MACRO(403, forbidden, "Forbidden")                                         \
    MACRO(404, not_found, "Not Found")                                         \
    MACRO(405, method_not_allowed, "Method Not Allowed")                       \
    MACRO(416, rng_not_satisfiable, "Requested range not satisfiable")         \
    MACRO(500, internal_server_error, "Internal Server Error")                 \
    MACRO(501, not_implemented, "Not Implemented")                             \
    MACRO(502, bad_gateway, "Bad Gateway")                                     \
    MACRO(503, service_unavailable, "Service Unavailable")

enum class http_status_code
{
#define STATUS_CODES_IT(code, name, _u) name = code,
    STATUS_CODES(STATUS_CODES_IT)
#undef STATUS_CODES_IT
};

inline const char* http_status_code_msg(http_status_code code)
{
    switch (code)
    {
#define STATUS_CODES_IT(_u, name, msg)                                         \
    case http_status_code::name:                                               \
        return msg;
        STATUS_CODES(STATUS_CODES_IT)
#undef STATUS_CODES_IT
    }
    return "";
}

} // namespace net
} // namespace x3me
