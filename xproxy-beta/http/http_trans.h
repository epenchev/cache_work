#pragma once

#include "http_msg_parser.h"

namespace cache
{
class cache_key;
} // namespace cache
////////////////////////////////////////////////////////////////////////////////
namespace http
{
struct all_stats;
struct req_msg;
struct resp_msg;

class http_trans
{
    using req_msg_t  = x3me::utils::pimpl<req_msg, 104, 8>;
    using resp_msg_t = x3me::utils::pimpl<resp_msg, 144, 8>;
    // Why flags and not state machine???
    // The main advantage of the state machine (IMO) is that you can
    // see the transition table in one place instead of tracing the
    // transitions through the code. This advantage here is greatly
    // reduced because the current state only matters in the on_data
    // methods.
    // In addition, here we need to use the state as flags in some of the
    // getter methods of the class. The state machine also needs more
    // boilerplate. Thus I decided to use flags here.
    using state_flags_t = uint16_t;
    enum state_flags : state_flags_t
    {
        flag_initial            = 0,
        flag_req_hdrs_complete  = 1 << 0,
        flag_req_complete_ok    = 1 << 1,
        flag_req_complete_eof   = 1 << 2,
        flag_resp_hdrs_complete = 1 << 3,
        flag_resp_complete_ok   = 1 << 4,
        flag_resp_complete_eof  = 1 << 5,
        flag_http_tunnel        = 1 << 6,
        flag_chunked            = 1 << 7,
        flag_head_request       = 1 << 8,
        flag_req_with_host      = 1 << 9,
        flag_cache_hit          = 1 << 10,
        flag_cache_miss         = 1 << 11,
        flag_cache_csum_miss    = 1 << 12,
        flag_done_error         = 1 << 13,
        flag_done_unsupported   = 1 << 14,
        flag_req_complete       = flag_req_complete_ok | flag_req_complete_eof,
        flag_resp_complete      = flag_resp_complete_ok | flag_resp_complete_eof,
        flag_done_forced        = flag_done_error | flag_done_unsupported,
    };
    friend constexpr state_flags& operator|=(state_flags& lhs,
                                             state_flags rhs) noexcept;
    friend constexpr state_flags operator|(state_flags lhs,
                                           state_flags rhs) noexcept;
    friend std::ostream& operator<<(std::ostream&, state_flags) noexcept;

private:
    http_msg_parser<http_trans, req_parser> req_parser_;
    http_msg_parser<http_trans, resp_parser> resp_parser_;

    req_msg_t req_msg_;
    resp_msg_t resp_msg_;

    id_tag tag_; // Needs const, but the move assignment doesn't allow

    // Message (headers + body) bytes received from the origin.
    // Remaining body bytes are received from cache.
    // If this remains 0 all response bytes has been received from the origin.
    bytes32_t origin_resp_bytes_ = 0;

    state_flags state_flags_ = flag_initial;
    // These flags tell us if we need to collect the current header
    // value or not. So far we have a hole at the end of the transaction,
    // thus we can use two separate flags, instead of merging them in one.
    bool collect_req_hdr_val_  = false;
    bool collect_resp_hdr_val_ = false;

public:
    enum struct res
    {
        proceed,
        complete,
        unsupported,
        error,
    };

    struct on_data_res
    {
        res res_;
        bytes32_t consumed_;
    };

public:
    explicit http_trans(const id_tag& tag) noexcept;
    ~http_trans() noexcept;

    http_trans(http_trans&& rhs) noexcept;
    http_trans& operator=(http_trans&& rhs) noexcept;

    http_trans(const http_trans&) = delete;
    http_trans& operator=(const http_trans&) = delete;

    void set_tag(const id_tag& tag) noexcept;
    const id_tag& tag() const noexcept { return tag_; }

    on_data_res on_req_data(const bytes8_t* data, bytes32_t size) noexcept;
    on_data_res on_resp_data(const bytes8_t* data, bytes32_t size) noexcept;

    void on_req_end_of_stream() noexcept;
    void on_resp_end_of_stream() noexcept;

    void force_http_tunnel() noexcept;

    bool req_hdrs_completed() const noexcept;
    bool resp_hdrs_completed() const noexcept;
    bool req_completed() const noexcept;
    bool resp_completed() const noexcept;

    bytes32_t req_hdrs_bytes() const noexcept;
    bytes32_t resp_hdrs_bytes() const noexcept;
    bytes64_t req_body_bytes() const noexcept;
    bytes64_t resp_body_bytes() const noexcept;
    bytes64_t req_bytes() const noexcept;
    bytes64_t resp_bytes() const noexcept;
    // The returned values are valid only after headers are completed in
    // the given direction, although the 'Content-Length' may get parsed earlier
    optional_t<bytes64_t> req_content_len() const noexcept;
    optional_t<bytes64_t> resp_content_len() const noexcept;

    // Returns false if the transaction is in unsupported or error state.
    // Returns true in all other cases.
    bool is_valid() const noexcept;

    // The return value is really valid after the response headers are completed
    bool is_chunked() const noexcept;
    // The return value is really valid after the response headers are completed
    bool is_keep_alive() const noexcept;
    bool in_http_tunnel() const noexcept;

    void set_cache_hit() noexcept;
    void set_cache_miss() noexcept;
    void set_cache_csum_miss() noexcept;
    bool is_cache_hit() const noexcept;

    void set_origin_resp_bytes(bytes32_t bytes) noexcept;
    bytes32_t origin_resp_bytes() const noexcept;

    string_view_t req_url() const noexcept;
    void set_cache_url(boost_string_t&& url) noexcept;

    // Returns valid cache key only after the response headers are completed
    // and the transaction is in normal mode (not http_tunnel, unsupported or
    // error).
    optional_t<cache::cache_key> get_cache_key() const noexcept;

    void log_before_destroy() const noexcept;

    // Used for statistics only. Must be called only once.
    void update_req_stats(all_stats& asts) const noexcept;
    void update_resp_stats(all_stats& asts) const noexcept;

public:
    // It's a bit ugly that the notifications needs to be part of the
    // public interface, because we use the parsers privately, but ...
    // Request notifications
    int on_msg_begin(req_parser) noexcept;
    int on_method(http_method m) noexcept;
    int on_url_begin() noexcept;
    int on_url_data(const char* d, size_t s) noexcept;
    int on_url_end() noexcept;
    int on_http_version(http_version v, req_parser) noexcept;
    int on_hdr_key_begin(req_parser) noexcept;
    int on_hdr_key_data(const char* d, size_t s, req_parser) noexcept;
    int on_hdr_key_end(req_parser) noexcept;
    int on_hdr_val_begin(req_parser) noexcept;
    int on_hdr_val_data(const char* d, size_t s, req_parser) noexcept;
    int on_hdr_val_end(req_parser) noexcept;
    int on_hdrs_end(req_parser) noexcept;
    int on_msg_end(req_parser) noexcept;
    // Response notifications
    int on_msg_begin(resp_parser) noexcept;
    int on_http_version(http::http_version v, resp_parser) noexcept;
    int on_status_code(http_status s) noexcept;
    int on_hdr_key_begin(resp_parser) noexcept;
    int on_hdr_key_data(const char* d, size_t s, resp_parser) noexcept;
    int on_hdr_key_end(resp_parser) noexcept;
    int on_hdr_val_begin(resp_parser) noexcept;
    int on_hdr_val_data(const char* d, size_t s, resp_parser) noexcept;
    int on_hdr_val_end(resp_parser) noexcept;
    int on_hdrs_end(resp_parser) noexcept;
    int on_trailing_hdrs_begin() noexcept;
    int on_trailing_hdrs_end() noexcept;
    int on_msg_end(resp_parser) noexcept;

private:
    void read_req_content_len() noexcept;
    void read_req_host() noexcept;

    bool read_resp_content_len() noexcept;
    void read_resp_transfer_enc() noexcept;
    void read_resp_content_enc() noexcept;
    void read_resp_content_md5() noexcept;
    bool read_resp_content_rng() noexcept;
    void read_resp_last_modified() noexcept;
    void read_resp_cache_control() noexcept;
    void read_resp_pragma() noexcept;
    void read_resp_digest() noexcept;
    void read_resp_etag() noexcept;

    friend std::ostream& operator<<(std::ostream& os,
                                    const http_trans& rhs) noexcept;
};

} // namespace http
