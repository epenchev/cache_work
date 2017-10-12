#include "precompiled.h"
#include "http_trans.h"
#include "http_date.h"
#include "http_msg.h"
#include "http_version.h"
#include "http_stats.h"
#include "cache/cache_key.h"
#include "hdr_values_store.ipp"

using x3me::print_utils::print_text;
using x3me::print_utils::print_lim_text;

namespace http
{
static void trim_string_view(string_view_t& sv) noexcept
{
    // Trim left
    sv.remove_prefix(std::min(sv.find_first_not_of(' '), sv.size()));
    // Trim right
    auto pos = sv.find_last_not_of(' ');
    if (pos != string_view_t::npos)
        pos += 1;
    sv.remove_suffix(sv.size() - std::min(pos, sv.size()));
}

static string_view_t url_no_protocol(const boost_string_t& url) noexcept
{
    constexpr string_view_t http{"http://", 7};
    return boost::istarts_with(url, http)
               ? string_view_t{url.data() + http.size(),
                               url.size() - http.size()}
               : string_view_t{url.data(), url.size()};
}

////////////////////////////////////////////////////////////////////////////////

constexpr http_trans::state_flags&
operator|=(http_trans::state_flags& lhs, http_trans::state_flags rhs) noexcept
{
    using sf_t = http_trans::state_flags_t;
    lhs        = (http_trans::state_flags)((sf_t)lhs | (sf_t)rhs);
    return lhs;
}

constexpr http_trans::state_flags
operator|(http_trans::state_flags lhs, http_trans::state_flags rhs) noexcept
{
    lhs |= rhs;
    return lhs;
}

std::ostream& operator<<(std::ostream& os, http_trans::state_flags rhs) noexcept
{
    if (rhs != http_trans::flag_initial)
    {
        if (rhs & http_trans::flag_req_hdrs_complete)
            os << "req_hdrs_complete;";
        if (rhs & http_trans::flag_req_complete_ok)
            os << "req_complete_ok;";
        if (rhs & http_trans::flag_req_complete_eof)
            os << "req_complete_eof;";
        if (rhs & http_trans::flag_resp_hdrs_complete)
            os << "resp_hdrs_complete;";
        if (rhs & http_trans::flag_resp_complete_ok)
            os << "resp_complete_ok;";
        if (rhs & http_trans::flag_resp_complete_eof)
            os << "resp_complete_eof;";
        if (rhs & http_trans::flag_http_tunnel)
            os << "http_tunnel;";
        if (rhs & http_trans::flag_chunked)
            os << "chunked;";
        if (rhs & http_trans::flag_cache_hit)
            os << "cache_hit;";
        else if (rhs & http_trans::flag_cache_miss)
            os << "cache_miss;";
        else if (rhs & http_trans::flag_cache_csum_miss)
            os << "cache_csum_miss;";
        if (rhs & http_trans::flag_done_error)
            os << "done_error;";
        if (rhs & http_trans::flag_done_unsupported)
            os << "done_unsupported;";
    }
    else
    {
        os << "initial;";
    }
    return os;
}
////////////////////////////////////////////////////////////////////////////////

http_trans::http_trans(const id_tag& tag) noexcept : req_parser_(*this),
                                                     resp_parser_(*this),
                                                     tag_(tag)
{
    XLOG_INFO(tag_, "Http_trans create. Curr_state '{}'", state_flags_);
}

http_trans::~http_trans() noexcept
{
}

http_trans::http_trans(http_trans&& rhs) noexcept
    : req_parser_(std::move(rhs.req_parser_)),
      resp_parser_(std::move(rhs.resp_parser_)),
      req_msg_(std::move(rhs.req_msg_)),
      resp_msg_(std::move(rhs.resp_msg_)),
      tag_(std::exchange(rhs.tag_, net_tag)),
      state_flags_(std::exchange(rhs.state_flags_, flag_initial)),
      collect_req_hdr_val_(std::exchange(rhs.collect_req_hdr_val_, false)),
      collect_resp_hdr_val_(std::exchange(rhs.collect_resp_hdr_val_, false))
{
    req_parser_.set_notified(*this);
    resp_parser_.set_notified(*this);
    rhs.req_parser_.set_notified(rhs);
    rhs.resp_parser_.set_notified(rhs);

    rhs.req_msg_  = req_msg_t{};
    rhs.resp_msg_ = resp_msg_t{};
}

http_trans& http_trans::operator=(http_trans&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        req_parser_           = std::move(rhs.req_parser_);
        resp_parser_          = std::move(rhs.resp_parser_);
        req_msg_              = std::move(rhs.req_msg_);
        resp_msg_             = std::move(rhs.resp_msg_);
        tag_                  = std::exchange(rhs.tag_, net_tag);
        state_flags_          = std::exchange(rhs.state_flags_, flag_initial);
        collect_req_hdr_val_  = std::exchange(rhs.collect_req_hdr_val_, false);
        collect_resp_hdr_val_ = std::exchange(rhs.collect_resp_hdr_val_, false);

        req_parser_.set_notified(*this);
        resp_parser_.set_notified(*this);
        rhs.req_parser_.set_notified(rhs);
        rhs.resp_parser_.set_notified(rhs);

        rhs.req_msg_  = req_msg{};
        rhs.resp_msg_ = resp_msg{};
    }
    return *this;
}

void http_trans::set_tag(const id_tag& tag) noexcept
{
    tag_ = tag;
}

////////////////////////////////////////////////////////////////////////////////

http_trans::on_data_res http_trans::on_req_data(const bytes8_t* data,
                                                bytes32_t size) noexcept
{
    X3ME_ASSERT(data && (size > 0));
    on_data_res ret{res::error, 0};
    const auto hdr_bytes = req_parser_.hdr_bytes();
    if (!(state_flags_ & (flag_req_complete | flag_done_forced)))
    {
        auto str             = reinterpret_cast<const char*>(data);
        const auto processed = req_parser_.execute(data, size);
        const auto err_code = req_parser_.get_error_code();
        if (err_code == HPE_OK)
        {
            const auto curr_hdr_bytes = req_parser_.hdr_bytes() - hdr_bytes;
            if (curr_hdr_bytes > 0) // Log the headers
            {
                // We use print_text here because part of the body could be
                // here too. However the print_text is kind of slow, because
                // it needs to inspect every symbol.
                XLOG_DEBUG(tag_, "Http_trans::on_req_data. Processed bytes {}. "
                                 "Curr_state '{}'. Req_hdrs:\n{}",
                           processed, state_flags_,
                           string_view_t{str, curr_hdr_bytes});
            }
            else
            {
                XLOG_TRACE(tag_, "Http_trans::on_req_data. Processed bytes {}. "
                                 "Curr_state '{}'",
                           processed, state_flags_);
            }
            ret.res_ = (state_flags_ & flag_req_complete) ? res::complete
                                                          : res::proceed;
        }
        else if (state_flags_ & flag_done_unsupported)
        {
            // Add 5 chars for more context in the log
            XLOG_INFO(tag_,
                      "Http_trans::on_req_data. Unsupported req. Req_data:\n{}",
                      print_lim_text(str, size, processed + 5));
            ret.res_ = res::unsupported;
        }
        // This error is seen way too often because a lot of people use
        // port 80 for binary data and thus we want to log this error with
        // different log level
        else if (err_code != http_errno::HPE_INVALID_METHOD)
        {
            // Add 5 chars for more context in the log
            XLOG_WARN(
                tag_,
                "Http_trans::on_req_data. Parser error '{}'. Req_data:\n{}",
                get_error_str(err_code),
                print_lim_text(str, size, processed + 5));
            state_flags_ |= flag_done_error;
        }
        else // if (err_code === http_errno::INVALID_METHOD)
        {
            // Add 5 chars for more context in the log
            XLOG_INFO(
                tag_,
                "Http_trans::on_req_data. Parser error '{}'. Req_data:\n{}",
                get_error_str(err_code),
                print_lim_text(str, size, processed + 5));
            state_flags_ |= flag_done_error;
        }
        ret.consumed_ = processed;
    }
    else
    {
        // We can't use assert here because we can enter here due to a
        // programmer error, or due to the fact that we have a request
        // without content-length but with data after the response.
        const auto& url      = req_msg_->url_;
        const bytes64_t clen = req_msg_->content_len_ != req_msg::no_len
                                   ? req_msg_->content_len_
                                   : 0;
        XLOG_ERROR(tag_, "Http_trans::on_req_data. Called in wrong state '{}'. "
                         "URL {}. Req Content-Length {}. Req_data:\n{}",
                   state_flags_, url, clen, print_lim_text(data, size, 50U));
        state_flags_ |= flag_done_error;
    }
    return ret;
}

http_trans::on_data_res http_trans::on_resp_data(const bytes8_t* data,
                                                 bytes32_t size) noexcept
{
    X3ME_ASSERT(data && (size > 0));
    on_data_res ret{res::error, 0};
    const auto hdr_bytes = resp_parser_.hdr_bytes();
    if (!(state_flags_ & (flag_resp_complete | flag_done_forced)))
    {
        auto str             = reinterpret_cast<const char*>(data);
        const auto processed = resp_parser_.execute(data, size);
        const auto err_code = resp_parser_.get_error_code();
        if (err_code == HPE_OK)
        {
            const auto curr_hdr_bytes = resp_parser_.hdr_bytes() - hdr_bytes;
            // More often we'll have response data than headers, thus this
            // branch is first here.
            if (!(curr_hdr_bytes > 0))
            {
                XLOG_TRACE(tag_,
                           "Http_trans::on_resp_data. Processed bytes {}. "
                           "Curr_state '{}'",
                           processed, state_flags_);
            }
            else // Log the headers
            {
                // We use print_text here because part of the body could be
                // here too. However the print_text is kind of slow, because
                // it needs to inspect every symbol.
                XLOG_DEBUG(tag_,
                           "Http_trans::on_resp_data. Processed bytes {}. "
                           "Curr_state '{}'. Resp_hdrs:\n{}",
                           processed, state_flags_,
                           string_view_t{str, curr_hdr_bytes});
            }
            ret.res_ = (state_flags_ & flag_resp_complete) ? res::complete
                                                           : res::proceed;
        }
        else if (state_flags_ & flag_done_unsupported)
        {
            // Add 5 chars for more context in the log
            XLOG_INFO(
                tag_,
                "Http_trans::on_resp_data. Unsupported resp. Resp_data:\n{}",
                print_lim_text(str, size, processed + 5));
            ret.res_ = res::unsupported;
        }
        else
        {
            // Add 5 chars for more context in the log
            XLOG_WARN(
                tag_,
                "Http_trans::on_resp_data. Parser error '{}'. Resp_data:\n{}",
                get_error_str(err_code),
                print_lim_text(str, size, processed + 5));
            state_flags_ |= flag_done_error;
        }
        ret.consumed_ = processed;
    }
    else
    {
        // We can't use assert here because we can enter here due to a
        // programmer error, or due to the fact that we have a response
        // without content-length but with data after the response.
        const auto& url      = req_msg_->url_;
        const bytes64_t clen = resp_msg_->content_len_ != resp_msg::no_len
                                   ? resp_msg_->content_len_
                                   : 0;
        XLOG_ERROR(tag_,
                   "Http_trans::on_resp_data. Called in wrong state '{}'. "
                   "URL {}. Resp Content-Length {}. Resp_data:\n{}",
                   state_flags_, url, clen, print_lim_text(data, size, 50U));
        state_flags_ |= flag_done_error;
    }
    return ret;
}

void http_trans::on_req_end_of_stream() noexcept
{
    if (!(state_flags_ & flag_req_complete))
    {
        XLOG_INFO(tag_, "Http_trans::on_req_end_of_stream. End of stream "
                        "before complete request. Curr_state '{}'",
                  state_flags_);
        state_flags_ |= (flag_req_complete_eof | flag_http_tunnel);
    }
}

void http_trans::on_resp_end_of_stream() noexcept
{
    if (!(state_flags_ & flag_resp_complete))
    {
        XLOG_INFO(tag_, "Http_trans::on_resp_end_of_stream. End of stream "
                        "before complete response. Curr_state '{}'",
                  state_flags_);
        state_flags_ |= (flag_resp_complete_eof | flag_http_tunnel);
    }
}

void http_trans::force_http_tunnel() noexcept
{
    XLOG_DEBUG(tag_, "Http_trans::force_http_tunnel. Curr_state '{}'",
               state_flags_);
    state_flags_ |= flag_http_tunnel;
}

////////////////////////////////////////////////////////////////////////////////

bool http_trans::req_hdrs_completed() const noexcept
{
    return (state_flags_ & flag_req_hdrs_complete);
}

bool http_trans::resp_hdrs_completed() const noexcept
{
    return (state_flags_ & flag_resp_hdrs_complete);
}

bool http_trans::req_completed() const noexcept
{
    return (state_flags_ & flag_req_complete);
}

bool http_trans::resp_completed() const noexcept
{
    return (state_flags_ & flag_resp_complete);
}

bytes32_t http_trans::req_hdrs_bytes() const noexcept
{
    return req_parser_.hdr_bytes();
}

bytes32_t http_trans::resp_hdrs_bytes() const noexcept
{
    return resp_parser_.hdr_bytes();
}

bytes64_t http_trans::req_body_bytes() const noexcept
{
    return req_parser_.msg_bytes() - req_parser_.hdr_bytes();
}

bytes64_t http_trans::resp_body_bytes() const noexcept
{
    return resp_parser_.msg_bytes() - resp_parser_.hdr_bytes();
}

bytes64_t http_trans::req_bytes() const noexcept
{
    return req_parser_.msg_bytes();
}

bytes64_t http_trans::resp_bytes() const noexcept
{
    return resp_parser_.msg_bytes();
}

optional_t<bytes64_t> http_trans::req_content_len() const noexcept
{
    optional_t<bytes64_t> r;
    if (req_msg_->content_len_ != req_msg::no_len)
        r = req_msg_->content_len_;
    return r;
}

optional_t<bytes64_t> http_trans::resp_content_len() const noexcept
{
    optional_t<bytes64_t> r;
    // The content length reported in case of head request doesn't correspond
    // to the response body length. Pretend that there is no content-length
    // set in this case, so that we don't confuse external functionality.
    if ((resp_msg_->content_len_ != resp_msg::no_len) &&
        !(state_flags_ & flag_head_request))
    {
        r = resp_msg_->content_len_;
    }
    return r;
}

bool http_trans::is_valid() const noexcept
{
    return ((state_flags_ & flag_done_forced) == 0);
}

bool http_trans::is_chunked() const noexcept
{
    return (state_flags_ & flag_chunked);
}

bool http_trans::is_keep_alive() const noexcept
{
    return req_parser_.is_keep_alive() && resp_parser_.is_keep_alive();
}

bool http_trans::in_http_tunnel() const noexcept
{
    return (state_flags_ & flag_http_tunnel);
}

void http_trans::set_cache_hit() noexcept
{
    state_flags_ |= flag_cache_hit;
}

void http_trans::set_cache_miss() noexcept
{
    state_flags_ |= flag_cache_miss;
}

void http_trans::set_cache_csum_miss() noexcept
{
    state_flags_ |= flag_cache_csum_miss;
}

bool http_trans::is_cache_hit() const noexcept
{
    return (state_flags_ & flag_cache_hit);
}

void http_trans::set_origin_resp_bytes(bytes32_t bytes) noexcept
{
    origin_resp_bytes_ = bytes;
}

bytes32_t http_trans::origin_resp_bytes() const noexcept
{
    if (origin_resp_bytes_ == 0)
    {
        // No origin message bytes has been explicitly set.
        // This means that all of the message bytes has come from the origin.
        return resp_bytes();
    }
    return origin_resp_bytes_;
}

string_view_t http_trans::req_url() const noexcept
{
    return to_string_view(req_msg_->url_);
}

void http_trans::set_cache_url(boost_string_t&& url) noexcept
{
    req_msg_->cache_url_ = std::move(url);
}

optional_t<cache::cache_key> http_trans::get_cache_key() const noexcept
{
    optional_t<cache::cache_key> ret;
    if ((state_flags_ & flag_resp_hdrs_complete) &&
        !(state_flags_ & (flag_done_forced | flag_http_tunnel)))
    {
        X3ME_ASSERT(!req_msg_->url_.empty() &&
                        (resp_msg_->content_len_ != req_msg::no_len),
                    "Invalid transaction state");
        // Some of the below could be empty, but it's cheaper just
        // to assign them, than to check and then assign them.
        const auto& s     = resp_msg_->values_;
        ret               = cache::cache_key{};
        ret->url_         = to_string_view(req_msg_->url_);
        ret->cache_url_   = to_string_view(req_msg_->cache_url_);
        ret->content_md5_ = s.value_pos_to_view(resp_msg_->content_md5_);
        ret->digest_sha1_ = s.value_pos_to_view(resp_msg_->digest_sha1_);
        ret->digest_md5_  = s.value_pos_to_view(resp_msg_->digest_md5_);
        ret->etag_        = s.value_pos_to_view(resp_msg_->etag_);

        if (!resp_msg_->rng_.valid())
            ret->obj_full_len_ = resp_msg_->content_len_;
        else
        {
            X3ME_ASSERT(resp_msg_->object_len_ != resp_msg::no_len,
                        "If we have range info this field must have been set");
            ret->obj_full_len_ = resp_msg_->object_len_;
            ret->rng_.beg_     = resp_msg_->rng_.beg_;
            ret->rng_.end_     = resp_msg_->rng_.end_;
        }
        ret->last_modified_      = resp_msg_->last_modified_;
        ret->resp_cache_control_ = resp_msg_->cache_control_;
        ret->content_encoding_ =
            s.value_pos_to_view(resp_msg_->content_encoding_);
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////

void http_trans::log_before_destroy() const noexcept
{
    if (!(state_flags_ & flag_done_forced))
    {
        // The squid logging will currently appear only in the squid log
        // file even if the debug logging is turned on.
        constexpr state_flags_t all_complete =
            flag_req_complete_ok | flag_resp_complete_ok;
        const char* state = ((state_flags_ & all_complete) == all_complete)
                                ? "Complete"
                                : "Incomplete";
        auto hit = [this]
        {
            // clang-format off
            if (state_flags_ & flag_cache_hit) return "HIT";
            if (state_flags_ & flag_cache_miss) return "MISS";
            if (state_flags_ & flag_cache_csum_miss) return "CSUM_MISS";
            return "SKIP_MISS";
            // clang-format on
        };
        const char* proto =
            (state_flags_ & flag_http_tunnel) ? "tunnel://" : "http://";
        const auto url = url_no_protocol(req_msg_->url_);
        XLOG_INFO_EXPL(main_log_chan, squid_log_id, tag_,
                       "{} {} {} {}{} {} {} -> {} {} {} {}", state, hit(),
                       get_method_str(req_parser_.get_method()), proto, url,
                       req_parser_.hdr_bytes(), req_parser_.msg_bytes(),
                       resp_parser_.get_status_code(), resp_msg_->rng_,
                       resp_parser_.hdr_bytes(), resp_parser_.msg_bytes());
    }
    XLOG_INFO(tag_, "Http_trans destroy. {}", *this);
}

void http_trans::update_req_stats(all_stats& asts) const noexcept
{
    X3ME_ASSERT(req_completed());
    asts.var_stats_.cnt_all_req_ += 1;
    asts.var_stats_.size_all_req_ += req_bytes();
}

void http_trans::update_resp_stats(all_stats& asts) const noexcept
{
    X3ME_ASSERT(resp_completed());
    const auto resp_size = resp_bytes();

    auto& sts = asts.var_stats_;
    switch (resp_parser_.get_status_code())
    {
    case HTTP_STATUS_OK:
        sts.cnt_all_resp_200_ += 1;
        sts.size_all_resp_200_ += resp_size;
        break;
    case HTTP_STATUS_PARTIAL_CONTENT:
        sts.cnt_all_resp_206_ += 1;
        sts.size_all_resp_206_ += resp_size;
        break;
    default:
        sts.cnt_all_resp_other_ += 1;
        sts.size_all_resp_other_ += resp_size;
        break;
    }

    asts.resp_size_stats_.record_stats(resp_hdrs_bytes(), resp_size);
}

////////////////////////////////////////////////////////////////////////////////

int http_trans::on_msg_begin(req_parser) noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_req_begin. Curr_state '{}'", state_flags_);
    return http::res_ok;
}

int http_trans::on_method(http_method m) noexcept
{
    switch (m)
    {
    case HTTP_CONNECT:
        XLOG_INFO(tag_, "Http_trans::on_req_method CONNECT. Start unsupported "
                        "mode. Curr_state '{}'",
                  state_flags_);
        state_flags_ |= flag_done_unsupported;
        return http::res_error; // Break the parsing
    case HTTP_GET:
        XLOG_DEBUG(tag_, "Http_trans::on_req_method GET. Curr_state '{}'",
                   state_flags_);
        return http::res_ok;
    case HTTP_HEAD:
        state_flags_ |= flag_head_request;
    // [[fallthrough]]
    default:
        XLOG_DEBUG(tag_,
                   "Http_trans::on_req_method. Start HTTP tunnel on method "
                   "'{}'. Curr_state '{}'",
                   get_method_str(m), state_flags_);
        state_flags_ |= flag_http_tunnel;
        return http::res_ok;
    }
}

int http_trans::on_url_begin() noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_req_url_begin. Curr_state '{}'",
               state_flags_);
    req_msg_->url_.clear(); // Not really needed but ...
    return http::res_ok;
}

int http_trans::on_url_data(const char* d, size_t s) noexcept
{
    // We append '...' at the end to mark partial URL
    static_assert(req_msg::max_url_len > 3, "");
    constexpr auto max_allowed_url_len = req_msg::max_url_len - 3;

    auto& url = req_msg_->url_;

    size_t append_size = 0;
    if (url.size() < max_allowed_url_len)
    {
        const auto free_size = max_allowed_url_len - url.size();
        append_size          = std::min(s, free_size);

        url.append(d, append_size);
    }
    XLOG_TRACE(
        tag_, "Http_trans::on_url_data '{}'. Skipped bytes {}. Curr_state '{}'",
        string_view_t{d, s}, s - append_size, state_flags_);
    if ((append_size < s) && (url.size() == max_allowed_url_len))
    {
        url.append("...", 3); // Mark the URL as partial
        XLOG_INFO(
            tag_,
            "Http_trans::on_url_data. Start HTTP tunnel on too long URL. {}",
            url);
        state_flags_ |= flag_http_tunnel;
    }

    return http::res_ok;
}

int http_trans::on_url_end() noexcept
{
    // We don't decode the possibly URL encoded string of the URL,
    // because the ATS doesn't seem to do it.
    // If needed we can do it here.
    // Decode the URL in-place. The URL encoding allows decode in-place.
    XLOG_TRACE(tag_, "Http_trans::on_req_url_end. URL {}. Curr_state '{}'",
               req_msg_->url_, state_flags_);
    return http::res_ok;
}

int http_trans::on_http_version(http_version v, req_parser) noexcept
{
    if ((v == http_version{1, 0}) || (v == http_version{1, 1}))
    {
        XLOG_TRACE(tag_, "Http_trans::on_req_http_version {}. Curr_state '{}'",
                   v, state_flags_);
    }
    else
    {
        state_flags_ |= flag_done_unsupported;
        XLOG_WARN(tag_, "Http_trans::on_req_http_version. Unsupported HTTP "
                        "version {}. Curr_state '{}'",
                  v, state_flags_);
        return http::res_error; // Break the parsing
    }
    return http::res_ok;
}

int http_trans::on_hdr_key_begin(req_parser) noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_req_key_begin. Curr_state '{}'",
               state_flags_);
    req_msg_->values_.start_key();
    return http::res_ok;
}

int http_trans::on_hdr_key_data(const char* d, size_t s, req_parser) noexcept
{
    if (!req_msg_->values_.append_key(d, s))
    {
        const auto key_info = req_msg_->values_.current_key();
        assert(!key_info.full_); // Must be trimmed, if the append fails
        // The data could be binary, thus we need to use the print_text
        XLOG_WARN(tag_, "Http_trans::on_req_key_data. Error on too long key. "
                        "Key_begin '{}'. Curr_key_data '{}'. Curr_state '{}'",
                  print_text(key_info.key_), print_text(d, s), state_flags_);
        state_flags_ |= flag_done_error; // Not really needed
        return http::res_error; // Break the parsing
    }
    return http::res_ok;
}

int http_trans::on_hdr_key_end(req_parser) noexcept
{
    using namespace detail;
    const auto key_info = req_msg_->values_.current_key();
    if (key_info.full_ && hdr_unsupported<intr_req_hdrs>(key_info.key_))
    {
        XLOG_INFO(tag_, "Http_trans::on_req_key_end. Start unsupported "
                        "mode on header '{}'. Curr_state '{}'",
                  key_info.key_, state_flags_);
        state_flags_ |= flag_done_unsupported;
        return http::res_error; // Break the parsing
    }

    XLOG_TRACE(tag_,
               "Http_trans::on_req_key_end. Key_begin '{}'. Curr_state '{}'",
               key_info.key_, state_flags_);

    return http::res_ok;
}

int http_trans::on_hdr_val_begin(req_parser) noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_req_val_begin. Curr_state '{}'",
               state_flags_);

    using namespace detail;
    const auto key_info = req_msg_->values_.current_key();
    // Collect host information if it's not already present in the url
    if (state_flags_ & flag_http_tunnel)
    {
        collect_req_hdr_val_ =
            key_info.full_ && (is_same_hdr(req_hdr::host, key_info.key_) &&
                               !boost::istarts_with(req_msg_->url_, "http://"));
        // We don't check if it starts with 'www' because the internal
        // nodejs parser doesn't allow this. It emits error.
    }
    else
    {
        collect_req_hdr_val_ =
            key_info.full_ &&
            (is_same_hdr(req_hdr::content_length, key_info.key_) ||
             (is_same_hdr(req_hdr::host, key_info.key_) &&
              !boost::istarts_with(req_msg_->url_, "http://")));
        // We don't check if it starts with 'www' because the internal
        // nodejs parser doesn't allow this. It emits error.
    }

    return http::res_ok;
}

int http_trans::on_hdr_val_data(const char* d, size_t s, req_parser) noexcept
{
    if (collect_req_hdr_val_ && !req_msg_->values_.append_value(d, s))
    {
        const auto key_info = req_msg_->values_.current_key();
        const auto val = req_msg_->values_.current_value_view();
        XLOG_WARN(tag_, "Http_trans::on_req_val_data. Start HTTP tunnel on too "
                        "long value '{}' + '{}' for key '{}'. Curr_state '{}'",
                  val, string_view_t{d, s}, key_info.key_, state_flags_);
        state_flags_ |= flag_http_tunnel;
        collect_req_hdr_val_ = false;
        req_msg_->values_.remove_current_value();
    }
    return http::res_ok;
}

int http_trans::on_hdr_val_end(req_parser) noexcept
{
    const auto key_info = req_msg_->values_.current_key();
    if (collect_req_hdr_val_)
    {
        collect_req_hdr_val_ = false;
        assert(key_info.full_);
        using namespace detail;
        if (is_same_hdr(req_hdr::content_length, key_info.key_))
        {
            read_req_content_len();
        }
        else if (is_same_hdr(req_hdr::host, key_info.key_))
        {
            read_req_host();
        }
        else
        {
            assert(false && "Must not collect values for not handled cases");
        }
        // Remove any uncommitted header value from the store
        req_msg_->values_.remove_current_value();
    }
    else
    {
        XLOG_TRACE(tag_,
                   "Http_trans::on_req_val_end. Hdr_key '{}'. Curr_state '{}'",
                   key_info.key_, state_flags_);
    }
    return http::res_ok;
}

int http_trans::on_hdrs_end(req_parser) noexcept
{
    XLOG_DEBUG(tag_, "Http_trans::on_req_hdrs_end. Curr_state '{}'",
               state_flags_);
    state_flags_ |= flag_req_hdrs_complete;
    if (!boost::starts_with(req_msg_->url_, "http://"))
    {
        if (!(state_flags_ & flag_req_with_host) &&
            (req_msg_->url_.empty() || (req_msg_->url_[0] == '/')))
        { // No host info in the URL, set the origin IP instead of a host
            x3me::utilities::string_builder_64 ip;
            ip << "http://" << tag_.server_ip();
            req_msg_->url_.insert(0, ip.data(), ip.size());
        }
        else
        {
            req_msg_->url_.insert(0, "http://");
        }
    }
    return http::res_ok;
}

int http_trans::on_msg_end(req_parser) noexcept
{
    XLOG_DEBUG(tag_, "Http_trans::on_req_end. Curr_state '{}'", state_flags_);
    state_flags_ |= flag_req_complete_ok;
    return http::res_ok;
}

////////////////////////////////////////////////////////////////////////////////

int http_trans::on_msg_begin(resp_parser) noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_resp_begin. Curr_state '{}'",
               state_flags_);
    return http::res_ok;
}

int http_trans::on_http_version(http::http_version v, resp_parser) noexcept
{
    if ((v == http_version{1, 0}) || (v == http_version{1, 1}))
    {
        XLOG_TRACE(tag_, "Http_trans::on_resp_http_version {}. Curr_state '{}'",
                   v, state_flags_);
    }
    else
    {
        state_flags_ |= flag_done_unsupported;
        XLOG_WARN(tag_, "Http_trans::on_resp_http_version. Unsupported HTTP "
                        "version {}. Curr_state '{}'",
                  v, state_flags_);
        return http::res_error; // Break the parsing
    }
    return http::res_ok;
}

int http_trans::on_status_code(http_status s) noexcept
{
    if ((s == HTTP_STATUS_OK) || (s == HTTP_STATUS_PARTIAL_CONTENT))
    {
        XLOG_DEBUG(tag_, "Http_trans::on_status_code '{} {}'. Curr_state '{}'",
                   s, get_status_str(s), state_flags_);
    }
    else
    {
        XLOG_DEBUG(
            tag_,
            "Http_trans::on_status_code. Start HTTP tunnel on status code "
            "'{} {}'. Curr_state '{}'",
            s, get_status_str(s), state_flags_);
        state_flags_ |= flag_http_tunnel;
    }
    return http::res_ok;
}

int http_trans::on_hdr_key_begin(resp_parser) noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_resp_key_begin. Curr_state '{}'",
               state_flags_);
    resp_msg_->values_.start_key();
    return http::res_ok;
}

int http_trans::on_hdr_key_data(const char* d, size_t s, resp_parser) noexcept
{
    if (!resp_msg_->values_.append_key(d, s))
    {
        const auto key_info = resp_msg_->values_.current_key();
        assert(!key_info.full_); // Must be trimmed, if the append fails
        XLOG_WARN(tag_, "Http_trans::on_resp_key_data. Error on too long key. "
                        "Key_begin '{}'. Curr_key_data '{}'. Curr_state '{}'",
                  print_text(key_info.key_), print_text(d, s), state_flags_);
        state_flags_ |= flag_done_error; // Not really needed
        return http::res_error; // Break the parsing
    }
    return http::res_ok;
}

int http_trans::on_hdr_key_end(resp_parser) noexcept
{
    using namespace detail;
    const auto key_info = resp_msg_->values_.current_key();
    if (key_info.full_ && hdr_unsupported<intr_resp_hdrs>(key_info.key_))
    {
        XLOG_INFO(tag_, "Http_trans::on_resp_key_end. Start unsupported "
                        "mode on header '{}'. Curr_state '{}'",
                  key_info.key_, state_flags_);
        state_flags_ |= flag_done_unsupported;
        return http::res_error; // Break the parsing
    }
    if (!(state_flags_ & flag_http_tunnel) && key_info.full_ &&
        is_same_hdr(resp_hdr::transfer_encoding, key_info.key_))
    {
        XLOG_INFO(tag_, "Http_trans::on_resp_key_end. Start HTTP tunnel on "
                        "header 'Transfer-Encoding'. Curr_state '{}'",
                  state_flags_);
        state_flags_ |= flag_http_tunnel;
    }
    else
    {
        XLOG_TRACE(
            tag_,
            "Http_trans::on_resp_key_end. Key_begin '{}'. Curr_state '{}'",
            key_info.key_, state_flags_);
    }

    return http::res_ok;
}

int http_trans::on_hdr_val_begin(resp_parser) noexcept
{
    XLOG_TRACE(tag_, "Http_trans::on_resp_val_begin. Curr_state '{}'",
               state_flags_);

    using namespace detail;
    const auto key_info = resp_msg_->values_.current_key();
    if (state_flags_ & flag_http_tunnel)
    {
        // We need only to find out if we have chunked content or the content
        // length. Thus we need to collect the values for these headers.
        collect_resp_hdr_val_ =
            key_info.full_ &&
            (is_same_hdr(resp_hdr::transfer_encoding, key_info.key_) ||
             is_same_hdr(resp_hdr::content_length, key_info.key_));
    }
    else
    {
        // We need to collect the values of the headers related to the
        // cache key. We also need to know if we have chunked content or not.
        collect_resp_hdr_val_ =
            key_info.full_ &&
            (is_same_hdr(resp_hdr::transfer_encoding, key_info.key_) ||
             is_same_hdr(resp_hdr::content_length, key_info.key_) ||
             is_same_hdr(resp_hdr::last_modified, key_info.key_) ||
             is_same_hdr(resp_hdr::content_encoding, key_info.key_) ||
             is_same_hdr(resp_hdr::content_md5, key_info.key_) ||
             is_same_hdr(resp_hdr::content_range, key_info.key_) ||
             is_same_hdr(resp_hdr::digest, key_info.key_) ||
             is_same_hdr(resp_hdr::etag, key_info.key_));
    }

    return http::res_ok;
}

int http_trans::on_hdr_val_data(const char* d, size_t s, resp_parser) noexcept
{
    if (collect_resp_hdr_val_ && !resp_msg_->values_.append_value(d, s))
    {
        const auto key_info = resp_msg_->values_.current_key();
        const auto val = resp_msg_->values_.current_value_view();
        XLOG_WARN(tag_,
                  "Http_trans::on_resp_val_data. Start HTTP tunnel on too "
                  "long value '{}' for key '{}'. Curr_state '{}'",
                  val, key_info.key_, state_flags_);
        state_flags_ |= flag_http_tunnel;
        collect_req_hdr_val_ = false;
        resp_msg_->values_.remove_current_value();
    }
    return http::res_ok;
}

int http_trans::on_hdr_val_end(resp_parser) noexcept
{
    int ret             = http::res_ok;
    const auto key_info = resp_msg_->values_.current_key();
    if (collect_resp_hdr_val_)
    {
        using namespace detail;

        collect_resp_hdr_val_ = false;
        assert(key_info.full_);

        if ((resp_msg_->content_len_ == resp_msg::no_len) &&
            is_same_hdr(resp_hdr::content_length, key_info.key_))
        {
            if (!read_resp_content_len())
                ret = http::res_error;
        }
        else if (!(state_flags_ & flag_chunked) &&
                 is_same_hdr(resp_hdr::transfer_encoding, key_info.key_))
        {
            read_resp_transfer_enc();
        }
        // We are interested in the cache key fields only if we are not
        // in HTTP tunnel mode
        else if (!(state_flags_ & flag_http_tunnel))
        {
            if (resp_msg_->content_encoding_.empty() &&
                is_same_hdr(resp_hdr::content_encoding, key_info.key_))
            {
                read_resp_content_enc();
            }
            else if (resp_msg_->content_md5_.empty() &&
                     is_same_hdr(resp_hdr::content_md5, key_info.key_))
            {
                read_resp_content_md5();
            }
            else if (!resp_msg_->rng_.valid() &&
                     is_same_hdr(resp_hdr::content_range, key_info.key_))
            {
                if (!read_resp_content_rng())
                    ret = http::res_error;
            }
            else if ((resp_msg_->last_modified_ == 0) &&
                     is_same_hdr(resp_hdr::last_modified, key_info.key_))
            {
                read_resp_last_modified();
            }
            else if ((resp_msg_->cache_control_ ==
                      cache::resp_cache_control::cc_not_present) &&
                     is_same_hdr(resp_hdr::cache_control, key_info.key_))
            {
                read_resp_cache_control();
            }
            else if ((resp_msg_->cache_control_ ==
                      cache::resp_cache_control::cc_not_present) &&
                     is_same_hdr(resp_hdr::pragma, key_info.key_))
            {
                read_resp_pragma();
            }
            else if (resp_msg_->digest_md5_.empty() &&
                     resp_msg_->digest_sha1_.empty() &&
                     is_same_hdr(resp_hdr::digest, key_info.key_))
            {
                read_resp_digest();
            }
            else if (resp_msg_->etag_.empty() &&
                     is_same_hdr(resp_hdr::etag, key_info.key_))
            {
                read_resp_etag();
            }
            else
            { // We may enter here if we have same header present more than once
                // We need to decide, if we have such warnings, whether to
                // use the first or the last value. Currently we use the first.
                const auto val = req_msg_->values_.current_value_view();
                XLOG_WARN(tag_, "Http_trans::on_resp_val_end. Non HTTP "
                                "tunnel. Parsed repeated hdr '{}: "
                                "{}'. Curr_state '{}'",
                          key_info.key_, val, state_flags_);
                // We don't need to store the unknown header value
            }
        }
        else
        { // We may enter here if we have same header present more than once
            const auto val = req_msg_->values_.current_value_view();
            XLOG_WARN(tag_, "Http_trans::on_resp_val_end. HTTP tunnel. Parsed "
                            "repeated hdr '{}: "
                            "{}'. Curr_state '{}'",
                      key_info.key_, val, state_flags_);
            // We don't need to store the unknown header value
        }
        // Remove any uncommitted header value from the store
        resp_msg_->values_.remove_current_value();
    }
    else
    {
        XLOG_TRACE(
            tag_,
            "Http_trans::on_resp_val_end. Parsed hdr '{}'. Curr_state '{}'",
            key_info.key_, state_flags_);
    }
    return ret;
}

int http_trans::on_hdrs_end(resp_parser) noexcept
{
    XLOG_DEBUG(
        tag_, "Http_trans::on_resp_hdrs_end. Resp_cont_len {}. Curr_state '{}'",
        resp_msg_->content_len_, state_flags_);
    state_flags_ |= flag_resp_hdrs_complete;

    if ((resp_msg_->content_len_ == resp_msg::no_len) &&
        !(state_flags_ & flag_http_tunnel))
    { // We'll probably never enter here, still let's expect the unexpected
        XLOG_INFO(tag_, "Http_trans::on_resp_hdrs_end. Start HTTP tunnel "
                        "on missing Content-Length. Curr_state '{}'",
                  state_flags_);
        state_flags_ |= flag_http_tunnel;
    }
    if (!(state_flags_ & flag_http_tunnel) &&
        !resp_msg_->content_encoding_.empty())
    {
        if (resp_parser_.get_status_code() == HTTP_STATUS_PARTIAL_CONTENT)
        {
            XLOG_INFO(tag_, "Http_trans::on_resp_hdrs_end. Start HTTP tunnel "
                            "on non-empty Content-Encoding and response 206");
            state_flags_ |= flag_http_tunnel;
        }
    }
    // Skip body if content length is not present and not chunked.
    // Otherwise the parser will never emit on_msg_end.
    return ((state_flags_ & flag_chunked) ||
            (resp_msg_->content_len_ != resp_msg::no_len))
               ? http::res_ok
               : http::res_skip_body;
}

int http_trans::on_trailing_hdrs_begin() noexcept
{
    XLOG_INFO(tag_, "Http_trans::on_trailing_hdrs_begin. Start unsupported "
                    "mode. Curr_state '{}'",
              state_flags_);
    state_flags_ |= flag_done_unsupported;
    return http::res_error; // Break the parsing
}

int http_trans::on_trailing_hdrs_end() noexcept
{
    X3ME_ASSERT(false, "Parsing must have been stopped on_trailing_hdrs_begin");
    return http::res_error;
}

int http_trans::on_msg_end(resp_parser) noexcept
{
    XLOG_DEBUG(tag_, "Http_trans::on_resp_end. Curr_state '{}'", state_flags_);
    state_flags_ |= flag_resp_complete_ok;
    return http::res_ok;
}

////////////////////////////////////////////////////////////////////////////////

void http_trans::read_req_content_len() noexcept
{
    auto val = req_msg_->values_.current_value_view();
    trim_string_view(val);
    // Something is fishy if the content length header is there,
    // but the values is empty. So, don't go to HTTP tunnel if
    // some good Cristian put content length 0, but go to HTTP tunnel
    // if the value is different than zero or empty or shitty.
    unsigned long long clen = 0;
    auto beg                = val.begin();
    namespace x3 = boost::spirit::x3;
    if (!x3::parse(beg, val.end(), x3::ulong_long, clen) ||
        (beg != val.end())) // The full value is not parsed
    {
        XLOG_WARN(tag_, "Http_trans::read_req_content_len. Start HTTP tunnel "
                        "on parse error 'Content-Length: {}'. Curr_state '{}'",
                  val, state_flags_);
        state_flags_ |= flag_http_tunnel;
    }
    else if (val.empty() || (clen > 0))
    {
        req_msg_->content_len_ = clen;
        XLOG_DEBUG(tag_, "Http_trans::read_req_content_len. Start HTTP tunnel "
                         "on 'Content-Length: {}'. Curr_state '{}'",
                   val, state_flags_);
        state_flags_ |= flag_http_tunnel;
    }
    else // clen = 0
    {
        req_msg_->content_len_ = clen;
        XLOG_TRACE(tag_, "Http_trans::read_req_content_len. 'Content-Length "
                         "{}'. Curr_state '{}'",
                   val, state_flags_);
    }
}

void http_trans::read_req_host() noexcept
{
    assert(!boost::starts_with(req_msg_->url_, "http://"));
    const auto val = req_msg_->values_.current_value_view();
    // We don't try to decode the host values, they should be correct.
    // We need to see if somebody will try to send Unicode symbols,
    // in the Host field. At least the browsers don't allow this, when
    // you enter Cyrillic domain, for example.
    XLOG_TRACE(tag_,
               "Http_trans::read_req_host. Read 'Host: {}'. Curr_state '{}'",
               val, state_flags_);
    if (!val.empty())
    {
        state_flags_ |= flag_req_with_host;
        // Append the read host to the beginning of the URL string even
        // if it will become longer than the max URL length.
        // Don't add the host if the URL already starts with it.
        if (!boost::starts_with(req_msg_->url_, val))
            req_msg_->url_.insert(req_msg_->url_.begin(), val.begin(),
                                  val.end());
    }
}

////////////////////////////////////////////////////////////////////////////////

bool http_trans::read_resp_content_len() noexcept
{
    bool ret      = true;
    uint64_t clen = 0;
    auto val = resp_msg_->values_.current_value_view();
    trim_string_view(val);
    auto beg     = val.begin();
    namespace x3 = boost::spirit::x3;
    if (x3::parse(beg, val.end(), x3::ulong_long, clen) &&
        (beg == val.end())) // The full value is parsed
    {
        XLOG_TRACE(tag_, "Http_trans::read_resp_content_len. Parsed hdr "
                         "'Content-Length: {}'. Curr_state '{}'",
                   val, state_flags_);
        resp_msg_->content_len_ = clen;
        if (resp_msg_->rng_.valid() && (resp_msg_->rng_.len() != clen))
        {
            XLOG_WARN(tag_,
                      "Http_trans::read_resp_content_len. Go to error state "
                      "on inconsistency in 'Content-Length: {}' vs "
                      "'Content-Range: {}'. Curr_state '{}'",
                      val, resp_msg_->rng_, state_flags_);
            state_flags_ |= flag_done_error; // Not really needed
            ret = false;
        }
    }
    else
    {
        XLOG_WARN(tag_, "Http_trans::read_resp_content_len. Go to error state "
                        "on parse error 'Content-Length: {}'. Curr_state '{}'",
                  val, state_flags_);
        state_flags_ |= flag_done_error; // Not really needed
        ret = false;
    }
    return ret;
}

void http_trans::read_resp_transfer_enc() noexcept
{
    const auto val = resp_msg_->values_.current_value_view();
    assert(state_flags_ & flag_http_tunnel);
    // The nodejs parser searches case sensitive and exactly
    // one word. We search for chunked. We'll see if we have
    // multiple transfer
    constexpr string_view_t chunked{"chunked", 7};
    if (boost::find_first(val, chunked))
    {
        if (val.size() != chunked.size())
        { // Well see if we have such cases in practice
            XLOG_WARN(tag_, "Http_trans::read_resp_transfer_enc. Chunked and "
                            "other values for 'Transfer-Encoding: {}'"
                            "Curr_state '{}'",
                      val, state_flags_);
        }
        state_flags_ |= flag_chunked;
    }
    XLOG_TRACE(tag_, "Http_trans::read_resp_transfer_enc. Parsed hdr "
                     "'Transfer-Encoding: {}'. Curr_state '{}'",
               val, state_flags_);
}

void http_trans::read_resp_content_enc() noexcept
{
    const auto vv                = resp_msg_->values_.current_value_view();
    resp_msg_->content_encoding_ = resp_msg_->values_.current_value_pos();
    resp_msg_->values_.commit_current_value();
    XLOG_TRACE(tag_, "Http_trans::read_resp_content_enc. Parsed hdr "
                     "'Content-Encoding: {}'. Curr_state '{}'",
               vv, state_flags_);
}

void http_trans::read_resp_content_md5() noexcept
{
    const auto vv           = resp_msg_->values_.current_value_view();
    resp_msg_->content_md5_ = resp_msg_->values_.current_value_pos();
    resp_msg_->values_.commit_current_value();
    XLOG_TRACE(tag_, "Http_trans::read_resp_content_md5. Parsed hdr "
                     "'Content-MD5: {}'. Curr_state '{}'",
               vv, state_flags_);
}

bool http_trans::read_resp_content_rng() noexcept
{
    bool ret          = true;
    namespace x3      = boost::spirit::x3;
    const auto val    = resp_msg_->values_.current_value_view();
    bytes64_t obj_len = 0;
    resp_msg::rng rng;
    // clang-format off
    auto rdbeg = [&](const auto& ctx){ rng.beg_ = x3::_attr(ctx); };
    auto rdend = [&](const auto& ctx){ rng.end_ = x3::_attr(ctx); };
    auto rdlen = [&](const auto& ctx){ obj_len = x3::_attr(ctx); };
    auto parser = x3::no_case[x3::lit("bytes")] >> 
                  x3::ulong_long[rdbeg] >> '-' >>
                  x3::ulong_long[rdend] >> '/' >>
                  x3::ulong_long[rdlen];
    // clang-format on
    auto beg = val.begin();
    // Parse silently consuming the spaces
    if (phrase_parse(beg, val.end(), parser, x3::ascii::space) &&
        (beg == val.end()))
    {
        if (((resp_msg_->content_len_ == resp_msg::no_len) ||
             (resp_msg_->content_len_ == rng.len())) &&
            rng.valid() && (rng.end_ < obj_len))
        {
            resp_msg_->rng_        = rng;
            resp_msg_->object_len_ = obj_len;
            XLOG_TRACE(tag_, "Http_trans::read_resp_content_rng. Parsed hdr "
                             "'Content-Range: {}'. Range {}. Obj_len {}. "
                             "Curr_state '{}'",
                       val, rng, obj_len, state_flags_);
        }
        else if ((resp_msg_->content_len_ != resp_msg::no_len) &&
                 (resp_msg_->content_len_ != rng.len()))
        {
            XLOG_WARN(tag_,
                      "Http_trans::read_resp_content_rng. Go to error state "
                      "on inconsistency in 'Content-Length: {}' vs "
                      "'Content-Range: {}'. Curr_state '{}'",
                      resp_msg_->content_len_, val, state_flags_);
            state_flags_ |= flag_done_error; // Not really needed
            ret = false;
        }
        else
        {
            assert(!rng.valid() || (rng.end_ >= obj_len));
            XLOG_WARN(tag_,
                      "Http_trans::read_resp_content_rng. Go to error state "
                      "on invalid 'Content-Range: {}'. Curr_state '{}'",
                      val, state_flags_);
            state_flags_ |= flag_done_error; // Not really needed
            ret = false;
        }
    }
    else
    {
        XLOG_WARN(tag_, "Http_trans::read_resp_content_rng. Start HTTP tunnel "
                        "on invalid 'Content-Range: {}'. Curr_state '{}'",
                  val, state_flags_);
        state_flags_ |= flag_http_tunnel;
    }
    return ret;
}

void http_trans::read_resp_last_modified() noexcept
{
    // This is a bit of a hack, but I don't want to use the fat boost::date_time
    // functionality. However, the C function needs zero terminated string
    // and thus we append zero here.
    // Note that the value is not preserved in the store anyway.
    if (X3ME_UNLIKELY(!resp_msg_->values_.append_value("\0", 1)))
    {
        // Just want to see if we have such cases and later will think how
        // to improve the situation here.
        XLOG_ERROR(
            tag_,
            "Http_trans::read_resp_last_modified. Can't append terminate zero");
        return;
    }
    const auto http_date     = resp_msg_->values_.current_value_view();
    const auto http_log_date = http_date.substr(0, http_date.size() - 1);
    if (const auto unix_time = detail::parse_http_date(http_date.data()))
    {
        const auto t = unix_time.value();
        XLOG_TRACE(tag_, "Http_trans::read_resp_last_modified. Parsed hdr "
                         "'Last-Modified: {}'. Timestamp {}",
                   http_log_date, t);
        resp_msg_->last_modified_ = t;
    }
    else
    {
        boost::crc_32_type calc;
        calc.process_bytes(http_date.data(), http_date.size() - 1);
        const auto crc = calc.checksum();
        // Later this will become warning or info.
        XLOG_INFO(tag_, "Http_trans::read_resp_last_modified. Can't parse "
                        "Last-Modified: {}. Set crc {}",
                  http_log_date, crc);
        resp_msg_->last_modified_ = crc;
    }
}

void http_trans::read_resp_cache_control() noexcept
{
    const auto cache_control = resp_msg_->values_.current_value_view();
    XLOG_TRACE(tag_, "Http_trans::read_resp_cache_control {}", cache_control);
    // A case insensitive compare is intentionally not used here because
    // it takes additional locale parameter and the locale operations are slow.
    // I don't want to pay the price here, for the 0.0001% of the cases when
    // the cache control value won't be with small caps. In this case,
    // it'll be marked as other, which is fine IMO.
    if (boost::equal(cache_control, "private"))
    {
        resp_msg_->cache_control_ = cache::resp_cache_control::cc_private;
    }
    else if (boost::equal(cache_control, "public"))
    {
        resp_msg_->cache_control_ = cache::resp_cache_control::cc_public;
    }
    else if (boost::equal(cache_control, "no-cache") ||
             boost::equal(cache_control, "no-store"))
    {
        resp_msg_->cache_control_ = cache::resp_cache_control::cc_no_cache;
    }
    else
    {
        // We count all unrecognized 'Cache-control' values.
        // In addition, we count here all combination between
        // some of the above and max-age= , smax-age=, etc.
        resp_msg_->cache_control_ = cache::resp_cache_control::cc_other;
    }
}

void http_trans::read_resp_pragma() noexcept
{
    const auto pragma_value = resp_msg_->values_.current_value_view();
    XLOG_TRACE(tag_, "Http_trans::read_resp_pragma {}", pragma_value);
    // There could be additional things in the 'Pragma' field, but if the
    // 'no-cache' is present at any position we want to count it.
    // We take small risk here not using case insensitive comparison.
    // However, such comparison requires locale operations which are slow.
    // Don't want to pay the price for 0.001% of the cases when the
    // 'no-cache' can be written with some capital letters.
    if (boost::find_first(pragma_value, "no-cache"))
        resp_msg_->cache_control_ = cache::resp_cache_control::cc_no_cache;
}

void http_trans::read_resp_digest() noexcept
{
    auto get_digest = [](const string_view_t& hdr_val,
                         const string_view_t& digest)
    {
        string_view_t res;
        const auto fnd = boost::ifind_first(hdr_val, digest);
        if (fnd)
        {
            const string_view_t remaining(fnd.end(), hdr_val.end() - fnd.end());
            const auto fnd2 = boost::find_first(remaining, ",");
            res = fnd2 ? string_view_t(remaining.begin(),
                                       fnd2.begin() - remaining.begin())
                       : remaining;
            trim_string_view(res);
        }
        return res;
    };
    // We need to go back from string_view to hdr_value_view
    auto get_sub_pos = [](const detail::hdr_value_pos& orig_pos,
                          const string_view_t& orig_val,
                          const string_view_t& substr)
    {
        const auto beg = substr.begin() - orig_val.begin();
        const auto end = beg + substr.size();
        return orig_pos.sub_pos(beg, end);
    };
    constexpr string_view_t sha{"sha=", 4};
    constexpr string_view_t md5{"md5=", 4};

    const auto vpos     = resp_msg_->values_.current_value_pos();
    const auto hdr_val  = resp_msg_->values_.current_value_view();
    const auto dig_sha1 = get_digest(hdr_val, sha);
    const auto dig_md5 = get_digest(hdr_val, md5);
    if (!dig_sha1.empty() && !dig_md5.empty())
    {
        XLOG_WARN(tag_, "Http_trans::read_resp_digest. TODO! Start HTTP tunnel "
                        "on multiple digests 'Digest: {}'. Curr_state '{}'",
                  hdr_val, state_flags_);
        state_flags_ |= flag_http_tunnel;
        // We don't want to commit/preserve the current header value here
    }
    else if (!dig_sha1.empty())
    {
        resp_msg_->digest_sha1_ = get_sub_pos(vpos, hdr_val, dig_sha1);
        XLOG_TRACE(tag_, "Http_trans::read_resp_digest. Parsed hdr "
                         "'Digest: {}'. Found sha1 '{}'. Curr_state '{}'",
                   hdr_val, dig_sha1, state_flags_);
        resp_msg_->values_.commit_current_value();
    }
    else if (!dig_md5.empty())
    {
        resp_msg_->digest_md5_ = get_sub_pos(vpos, hdr_val, dig_md5);
        XLOG_TRACE(tag_, "Http_trans::read_resp_digest. Parsed hdr "
                         "'Digest: {}'. Found md5 '{}'. Curr_state '{}'",
                   hdr_val, dig_md5, state_flags_);
        resp_msg_->values_.commit_current_value();
    }
    else
    {
        XLOG_TRACE(tag_, "Http_trans::read_resp_digest. Parsed hdr "
                         "'Digest: {}'. No interesting digest. Curr_state '{}'",
                   hdr_val, state_flags_);
        // We don't want to commit/preserve the current header value here
    }
}

void http_trans::read_resp_etag() noexcept
{
    const auto vv    = resp_msg_->values_.current_value_view();
    resp_msg_->etag_ = resp_msg_->values_.current_value_pos();
    resp_msg_->values_.commit_current_value();
    XLOG_TRACE(tag_, "Http_trans::read_resp_etag. Parsed hdr "
                     "'ETag: {}'. Curr_state '{}'",
               vv, state_flags_);
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const http_trans& rhs) noexcept
{
    const char* proto = (rhs.state_flags_ & http_trans::flag_http_tunnel)
                            ? "tunnel://"
                            : "http://";
    const auto url = url_no_protocol(rhs.req_msg_->url_);
    // clang-format off
    return os << 
        "{Method '" << get_method_str(rhs.req_parser_.get_method()) <<
        "'. URL '" << proto << url <<
        "'. Cache_URL '" << rhs.req_msg_->cache_url_ <<
        "'. Status " << rhs.resp_parser_.get_status_code() <<
        ". Req_bytes " << rhs.req_parser_.hdr_bytes() << ' ' <<
                          rhs.req_parser_.msg_bytes() <<
        ". Resp_bytes " << rhs.resp_parser_.hdr_bytes() << ' ' <<
                           rhs.resp_parser_.msg_bytes() <<
        ". State " << rhs.state_flags_ << '}';
    // clang-format on
}

} // namespace http
