#pragma once

#include "http_parser_type.h"

namespace http
{
class http_version;

namespace psm // parser state machine
{
enum struct parser_state : uint16_t;
template <typename Ntf, typename PT, typename Event>
int process_event(http_parser*, const Event&) noexcept;
} // namespace psm
////////////////////////////////////////////////////////////////////////////////
// Result which can be used by the callbacks for convenience
enum cb_res
{
    res_error     = -1,
    res_ok        = 0,
    res_skip_body = 1,
};

////////////////////////////////////////////////////////////////////////////////

#define ENABLE_FOR(parser_type)                                                \
    template <bool b   = true,                                                 \
              typename = std::enable_if_t<                                     \
                  b && std::is_same<ParserType, parser_type>::value>>

// TODO Write documentation when everything is set in stone.
template <typename Notified, typename ParserType>
class http_msg_parser
{
    static_assert(
        std::is_same<ParserType, req_parser>::value ||
            std::is_same<ParserType, resp_parser>::value,
        "The parser type must be either 'req_parser' or 'resp_parser'");

    template <typename Ntf, typename PT, typename Event>
    friend int psm::process_event(http_parser*, const Event&) noexcept;

    Notified* ntf_;

    http_parser impl_;

    bytes64_t msg_bytes_;
    // The header bytes are set once all headers are completed.
    // Up to this point the msg_bytes_ are equal to the hdr_bytes_.
    bytes32_t hdr_bytes_;

    psm::parser_state curr_state_;

    static http_parser_settings settings_;

public:
    explicit http_msg_parser(Notified& notified) noexcept;
    ~http_msg_parser() noexcept = default;

    http_msg_parser(http_msg_parser&& rhs) noexcept;
    http_msg_parser& operator=(http_msg_parser&& rhs) noexcept;

    http_msg_parser(const http_msg_parser&) = delete;
    http_msg_parser& operator=(const http_msg_parser&) = delete;

    void set_notified(Notified& ntf) noexcept { ntf_ = &ntf; }

    // This method must not be called from inside the notifications
    void reset() noexcept;

    bytes32_t execute(const bytes8_t* data, bytes32_t size) noexcept;

    http_version get_http_version() const noexcept;

    ENABLE_FOR(req_parser) http_method get_method() const noexcept
    {
        return static_cast<http_method>(impl_.method);
    }

    ENABLE_FOR(resp_parser) http_status get_status_code() const noexcept
    {
        return static_cast<http_status>(impl_.status_code);
    }

    http_errno get_error_code() const noexcept
    {
        return static_cast<http_errno>(impl_.http_errno);
    }

    bool is_keep_alive() const noexcept
    {
        return ::http_should_keep_alive(&impl_);
    }

    ENABLE_FOR(req_parser) bool is_upgrade() const noexcept
    {
        return impl_.upgrade;
    }

    // These methods won't return up to date results if they are called
    // from inside the notifications. The counters are updated before the
    // exit from the execute method.
    auto hdr_bytes() const { return hdr_bytes_ != 0 ? hdr_bytes_ : msg_bytes_; }
    auto msg_bytes() const { return msg_bytes_; }
};

#undef ENABLE_FOR

////////////////////////////////////////////////////////////////////////////////

const char* get_method_str(http_method m) noexcept;
const char* get_status_str(http_status s) noexcept;
const char* get_error_str(http_errno e) noexcept;

} // namespace http
