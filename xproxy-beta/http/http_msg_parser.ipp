#pragma once

#include "http_version.h"
// This is the implementation of the http_msg_parser.
// The http_msg_parser.h must be included before the inclusion of this file.
//
namespace http
{
namespace psm
{

////////////////////////////////////////////////////////////////////////////////
// Few helper functions which makes the proces_event (state machine) shorter.
// TODO The type_switch can also be made with 'constexpr if'.

template <typename F, typename... G>
struct type_switch_t : type_switch_t<F>::type, type_switch_t<G...>::type
{
    using type = type_switch_t;
    using type_switch_t<F>::type::operator();
    using type_switch_t<G...>::type::operator();

    template <typename F_, typename... G_>
    explicit type_switch_t(F_&& f, G_&&... g) noexcept
        : type_switch_t<F>::type(std::forward<F>(f)),
          type_switch_t<G...>::type(std::forward<G_&&>(g)...)
    {
    }
};

template <typename F>
struct type_switch_t<F>
{
    using type = F;
};

template <typename... F>
static auto type_switch(F&&... f) noexcept
{
    return type_switch_t<std::remove_reference_t<F>...>(std::forward<F>(f)...);
}

template <typename E>
static auto event(E) // A bit hacky way to convert type to string
{
    return __PRETTY_FUNCTION__; // GCC/Clang specific macro
}

////////////////////////////////////////////////////////////////////////////////
// The below helper functions with tag dispatch are needed until the
// 'if constexpr' functionality comes to us with C++17.
// The functionality is needed because we don't want to force the parser
// user to provide on_status_code method for request parser, on_method
// notification for response parser, etc.

template <typename Ntf>
int on_method_if(req_parser, Ntf* ntf,
                 const http_msg_parser<Ntf, req_parser>* p) noexcept
{
    return ntf->on_method(p->get_method());
}

template <typename Ntf>
int on_method_if(resp_parser, Ntf*,
                 const http_msg_parser<Ntf, resp_parser>*) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf>
int on_status_code_if(req_parser, Ntf*,
                      const http_msg_parser<Ntf, req_parser>*) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf>
int on_status_code_if(resp_parser, Ntf* ntf,
                      const http_msg_parser<Ntf, resp_parser>* p) noexcept
{
    return ntf->on_status_code(p->get_status_code());
}

template <typename Ntf>
int on_url_begin_if(req_parser, Ntf* ntf) noexcept
{
    return ntf->on_url_begin();
}

template <typename Ntf>
int on_url_begin_if(resp_parser, Ntf*) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf>
int on_url_data_if(req_parser, Ntf* ntf, const char* d, size_t s) noexcept
{
    return ntf->on_url_data(d, s);
}

template <typename Ntf>
int on_url_data_if(resp_parser, Ntf*, const char*, size_t) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf>
int on_url_end_if(req_parser, Ntf* ntf) noexcept
{
    return ntf->on_url_end();
}

template <typename Ntf>
int on_url_end_if(resp_parser, Ntf*) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf>
int on_trailing_hdrs_begin_if(resp_parser, Ntf* ntf) noexcept
{
    return ntf->on_trailing_hdrs_begin();
}

template <typename Ntf>
int on_trailing_hdrs_begin_if(req_parser, Ntf*) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf>
int on_trailing_hdrs_end_if(resp_parser, Ntf* ntf) noexcept
{
    return ntf->on_trailing_hdrs_end();
}

template <typename Ntf>
int on_trailing_hdrs_end_if(req_parser, Ntf*) noexcept
{
    assert(false);
    return res_error;
}

template <typename Ntf, typename ParserType>
int on_hdrs_end_pause(Ntf* ntf, http_parser& p, ParserType) noexcept
{
    // Pause the parser, so we can get the headers length
    ::http_parser_pause(&p, 1);
    return ntf->on_hdrs_end(ParserType{});
}

template <typename Ntf, typename ParserType>
int on_msg_end_pause(Ntf* ntf, http_parser& p, ParserType) noexcept
{
    // Pause the parser, so that we can finish transactions, etc.
    ::http_parser_pause(&p, 1);
    return ntf->on_msg_end(ParserType{});
}

////////////////////////////////////////////////////////////////////////////////
// States functionality

#define PARSER_STATES(XX)                                                      \
    XX(start)                                                                  \
    XX(msg_begin)                                                              \
    XX(url_data)                                                               \
    XX(hkey_data)                                                              \
    XX(hval_data)                                                              \
    XX(hdrs_end)                                                               \
    XX(msg_end)                                                                \
    XX(error)

enum struct parser_state : uint16_t
{
#define XX(name) name,
    PARSER_STATES(XX)
#undef XX
};

static std::ostream& operator<<(std::ostream& os,
                                const parser_state& rhs) noexcept
{
    switch (rhs)
    {
#define XX(name)                                                               \
    case parser_state::name:                                                   \
        return os << #name;
        PARSER_STATES(XX)
#undef XX
    }
    return os;
}

////////////////////////////////////////////////////////////////////////////////
// Events functionality

// Used to decrease template instantiations of the type switch
struct ev_base
{
};

struct ev_with_data : ev_base
{
    const char* data_;
    size_t size_;
    ev_with_data(const char* d, size_t s) : data_(d), size_(s) {}
};

struct ev_msg_begin : ev_base
{
    constexpr static auto next_state = parser_state::msg_begin;
};
struct ev_hdrs_end : ev_base
{
    constexpr static auto next_state = parser_state::hdrs_end;
};
struct ev_msg_end : ev_base
{
    constexpr static auto next_state = parser_state::msg_end;
};
struct ev_url_data : ev_with_data
{
    constexpr static auto next_state = parser_state::url_data;
    using ev_with_data::ev_with_data;
};
struct ev_hkey_data : ev_with_data
{
    constexpr static auto next_state = parser_state::hkey_data;
    using ev_with_data::ev_with_data;
};
struct ev_hval_data : ev_with_data
{
    constexpr static auto next_state = parser_state::hval_data;
    using ev_with_data::ev_with_data;
};

////////////////////////////////////////////////////////////////////////////////
// The state machine functionality.
// This is the closest thing that I could figure out, which is like the
// transition tables of present state machine libraries.
// Why don't we use here a present state machine library like boost::sml or
// boost::msm. They just add too much space overhead (44 bytes in this case).
// The current state machine needs only 16 bytes which fit in a hole in the
// parser and thus it's as if the SM doesn't occupy space at all.
// The function here is not so nice as the transition table description of the
// above libraries, but they have more boilerplate in other places.

#define RET_ERR(fun)                                                           \
    if ((fun) == res_error)                                                    \
    return (int)res_error

template <typename Ntf, typename PT, typename Event>
static int process_event(http_parser* p, const Event& ev) noexcept
{
    int res = res_error;
    auto hp = static_cast<http_msg_parser<Ntf, PT>*>(p->data);

    auto on_unexp_event = [](auto* hp, const auto& ev)
    {
        std::cerr << "BUG in HTTP Parser! No transition from state '"
                  << hp->curr_state_ << "' on '" << event(ev) << "'\n";
        ::abort();
        return res_error;
    };
    // Set the next state on exit.
    X3ME_SCOPE_EXIT { hp->curr_state_ = ev.next_state; };
    switch (hp->curr_state_)
    {
    case parser_state::start:
        res = hp->ntf_->on_msg_begin(PT{});
        break;
    case parser_state::msg_begin:
        res = type_switch(
            // On first receive of url data notify for method,
            // for url begin and for url data. Only for requests.
            [](auto* hp, const ev_url_data& ev)
            {
                RET_ERR(on_method_if(PT{}, hp->ntf_, hp));
                RET_ERR(on_url_begin_if(PT{}, hp->ntf_));
                return on_url_data_if(PT{}, hp->ntf_, ev.data_, ev.size_);
            },
            // In case of response when we start receive headers we need to
            // notify for the http_version, status, key begin and key data.
            [](auto* hp, const ev_hkey_data& ev)
            {
                RET_ERR(
                    hp->ntf_->on_http_version(hp->get_http_version(), PT{}));
                RET_ERR(on_status_code_if(PT{}, hp->ntf_, hp));
                RET_ERR(hp->ntf_->on_hdr_key_begin(PT{}));
                return hp->ntf_->on_hdr_key_data(ev.data_, ev.size_, PT{});
            },
            // In case of response without any headers we need to notify for
            // the http_version, status and headers end.
            [](auto* hp, const ev_hdrs_end&)
            {
                RET_ERR(
                    hp->ntf_->on_http_version(hp->get_http_version(), PT{}));
                RET_ERR(on_status_code_if(PT{}, hp->ntf_, hp));
                return on_hdrs_end_pause(hp->ntf_, hp->impl_, PT{});

            },
            // If a request or response starts with one or more \r or \n
            // symbols the nodejs parser issues mulitple on_message_begin events
            [](auto*, const ev_msg_begin&)
            {
                return res_ok;
            },
            on_unexp_event)(hp, ev);
        break;
    case parser_state::url_data:
        res = type_switch(
            // On next receive for url data just notify for url data.
            [](auto* hp, const ev_url_data& ev)
            {
                return on_url_data_if(PT{}, hp->ntf_, ev.data_, ev.size_);
            },
            // If no headers are present at all after the url
            [](auto* hp, const ev_hdrs_end&)
            {
                RET_ERR(on_url_end_if(PT{}, hp->ntf_));
                RET_ERR(
                    hp->ntf_->on_http_version(hp->get_http_version(), PT{}));
                return on_hdrs_end_pause(hp->ntf_, hp->impl_, PT{});
            },
            // On first header key data notify for url end, for http version,
            // for beginning of a header key and for the header key data.
            [](auto* hp, const ev_hkey_data& ev)
            {
                RET_ERR(on_url_end_if(PT{}, hp->ntf_));
                RET_ERR(
                    hp->ntf_->on_http_version(hp->get_http_version(), PT{}));
                RET_ERR(hp->ntf_->on_hdr_key_begin(PT{}));
                return hp->ntf_->on_hdr_key_data(ev.data_, ev.size_, PT{});
            },
            on_unexp_event)(hp, ev);
        break;
    case parser_state::hkey_data:
        res = type_switch(
            // On following header key data, just notify for the data
            [](auto* hp, const ev_hkey_data& ev)
            {
                return hp->ntf_->on_hdr_key_data(ev.data_, ev.size_, PT{});
            },
            // When header value data start notify for end of a header key,
            // beginning of a header value and header value data.
            [](auto* hp, const ev_hval_data& ev)
            {
                RET_ERR(hp->ntf_->on_hdr_key_end(PT{}));
                RET_ERR(hp->ntf_->on_hdr_val_begin(PT{}));
                return hp->ntf_->on_hdr_val_data(ev.data_, ev.size_, PT{});
            },
            on_unexp_event)(hp, ev);
        break;
    case parser_state::hval_data:
        res = type_switch(
            // On following header value data, just notify for the data
            [](auto* hp, const ev_hval_data& ev)
            {
                return hp->ntf_->on_hdr_val_data(ev.data_, ev.size_, PT{});
            },
            // On next header key notify for end of the header value,
            // begin of a header key and for the header key data.
            [](auto* hp, const ev_hkey_data& ev)
            {
                RET_ERR(hp->ntf_->on_hdr_val_end(PT{}));
                RET_ERR(hp->ntf_->on_hdr_key_begin(PT{}));
                return hp->ntf_->on_hdr_key_data(ev.data_, ev.size_, PT{});
            },
            // When headers end while we parse header value, we need to notify
            // for the end of the current value and for headers end.
            [](auto* hp, const ev_hdrs_end&)
            {
                RET_ERR(hp->ntf_->on_hdr_val_end(PT{}));
                return on_hdrs_end_pause(hp->ntf_, hp->impl_, PT{});
            },
            // On message end without headers complete means that we have
            // received footers after chunked data, and now after the footers
            // we get on_message_end
            [](auto* hp, const ev_msg_end&)
            {
                // Asserting this is a bit of a hack because we use
                // internal knowledge of the nodejs parser.
                assert(hp->impl_.flags & F_CHUNKED);
                RET_ERR(hp->ntf_->on_hdr_val_end(PT{}));
                RET_ERR(on_trailing_hdrs_end_if(PT{}, hp->ntf_));
                return on_msg_end_pause(hp->ntf_, hp->impl_, PT{});
            },
            on_unexp_event)(hp, ev);
        break;
    case parser_state::hdrs_end:
        res = type_switch(
            // When the body ends we are done. It may not have body at all.
            [](auto* hp, const ev_msg_end&)
            {
                return on_msg_end_pause(hp->ntf_, hp->impl_, PT{});
            },
            // On header key after headers complete, means that we have
            // trailing footers after chunked data.
            [](auto* hp, const ev_hkey_data& ev)
            {
                // Asserting this is a bit of a hack because we use
                // internal knowledge of the nodejs parser.
                assert(hp->impl_.flags & F_CHUNKED);
                RET_ERR(on_trailing_hdrs_begin_if(PT{}, hp->ntf_));
                RET_ERR(hp->ntf_->on_hdr_key_begin(PT{}));
                return hp->ntf_->on_hdr_key_data(ev.data_, ev.size_, PT{});
            },
            on_unexp_event)(hp, ev);
        break;
    // Parser called after finished and without reset.
    case parser_state::msg_end:
    // Parser called in error state and without reset.
    case parser_state::error:
    // New state added but not covered.
    default:
        res = on_unexp_event(hp, ev);
        break;
    }
    return res;
}
#undef RET_ERR

} // namespace psm
////////////////////////////////////////////////////////////////////////////////

template <typename Ntf, typename PT, typename Ev>
static int parser_cb(http_parser* p) noexcept
{
    return psm::process_event<Ntf, PT>(p, Ev{});
}

template <typename Ntf, typename PT, typename Ev>
static int parser_data_cb(http_parser* p, const char* data,
                          size_t size) noexcept
{
    return psm::process_event<Ntf, PT>(p, Ev{data, size});
}

////////////////////////////////////////////////////////////////////////////////

template <typename Ntf, typename PT>
static http_parser_settings init_settings() noexcept
{
    http_parser_settings sts;
    ::http_parser_settings_init(&sts);
    sts.on_message_begin    = &parser_cb<Ntf, PT, psm::ev_msg_begin>;
    sts.on_headers_complete = &parser_cb<Ntf, PT, psm::ev_hdrs_end>;
    sts.on_message_complete = &parser_cb<Ntf, PT, psm::ev_msg_end>;
    sts.on_header_field     = &parser_data_cb<Ntf, PT, psm::ev_hkey_data>;
    sts.on_header_value     = &parser_data_cb<Ntf, PT, psm::ev_hval_data>;
    // TODO Change with 'if constexpr' when present
    if (std::is_same<PT, req_parser>::value)
        sts.on_url = &parser_data_cb<Ntf, PT, psm::ev_url_data>;
    return sts;
}

template <typename Ntf, typename PT>
http_parser_settings
    http_msg_parser<Ntf, PT>::settings_ = init_settings<Ntf, PT>();

////////////////////////////////////////////////////////////////////////////////

template <typename Ntf, typename PT>
http_msg_parser<Ntf, PT>::http_msg_parser(Ntf& notified) noexcept
    : ntf_(&notified)
{
    reset();
}

template <typename Ntf, typename PT>
http_msg_parser<Ntf, PT>::http_msg_parser(http_msg_parser&& rhs) noexcept
    : ntf_(rhs.ntf_),
      impl_(rhs.impl_),
      msg_bytes_(rhs.msg_bytes_),
      hdr_bytes_(rhs.hdr_bytes_),
      curr_state_(rhs.curr_state_)
{
    impl_.data = this;

    rhs.reset();
}

template <typename Ntf, typename PT>
http_msg_parser<Ntf, PT>& http_msg_parser<Ntf, PT>::
operator=(http_msg_parser&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        ntf_        = rhs.ntf_;
        impl_       = rhs.impl_;
        msg_bytes_  = rhs.msg_bytes_;
        hdr_bytes_  = rhs.hdr_bytes_;
        curr_state_ = rhs.curr_state_;
        impl_.data  = this;

        rhs.ntf_ = nullptr;
        rhs.reset();
    }
    return *this;
}

template <typename Ntf, typename PT>
void http_msg_parser<Ntf, PT>::reset() noexcept
{
    struct get_http_parser_type
    {
        http_parser_type operator()(req_parser) { return HTTP_REQUEST; }
        http_parser_type operator()(resp_parser) { return HTTP_RESPONSE; }
    };
    ::http_parser_init(&impl_, get_http_parser_type()(PT{}));
    impl_.data = this;

    msg_bytes_ = 0;
    hdr_bytes_ = 0;

    curr_state_ = psm::parser_state::start;
}

template <typename Ntf, typename PT>
bytes32_t http_msg_parser<Ntf, PT>::execute(const bytes8_t* data,
                                            bytes32_t size) noexcept
{

    auto dt       = reinterpret_cast<const char*>(data);
    bytes32_t res = ::http_parser_execute(&impl_, &settings_, dt, size);

    constexpr auto max_msg = std::numeric_limits<decltype(msg_bytes_)>::max();
    X3ME_ENFORCE((max_msg - res) > msg_bytes_); // Don't overflow
    msg_bytes_ += res;

    if (get_error_code() == HPE_PAUSED)
    {
        switch (curr_state_)
        {
        case psm::parser_state::hdrs_end:
        {
            assert(hdr_bytes_ == 0); // Hdr bytes must be set only once
            // The nodejs parser when paused in headers_completed returns
            // processed bytes lesser with 1 byte, because the final '\n' is
            // still not consumed. Thus we need to add 1 byte.
            hdr_bytes_ = msg_bytes_ + 1;
            ::http_parser_pause(&impl_, 0); // Un-pause the parser
            res += execute(data + res, size - res);
            break;
        }
        case psm::parser_state::msg_end:
            // Un-pause the parser otherwise it won't report OK if asked
            // for the error_code.
            ::http_parser_pause(&impl_, 0);
            break;
        default:
            ::abort();
            break;
        }
    }

    if (get_error_code() != HPE_OK)
        curr_state_ = psm::parser_state::error;

    return res;
}

template <typename Ntf, typename PT>
http_version http_msg_parser<Ntf, PT>::get_http_version() const noexcept
{
    return http_version{impl_.http_major, impl_.http_minor};
}

////////////////////////////////////////////////////////////////////////////////
// NOTE The schema for these three free functions won't work if the .ipp file
// is added in more than one .cpp file. There will be multiple definitions
// of these functions in this case.

const char* get_method_str(http_method m) noexcept
{
    return ::http_method_str(m);
}

const char* get_status_str(http_status s) noexcept
{
    switch (s)
    {
#define XX(num, name, string)                                                  \
    case num:                                                                  \
        return #string;

        HTTP_STATUS_MAP(XX)

#undef XX
    }
    return "Uknown HTTP Status Code";
}

const char* get_error_str(http_errno e) noexcept
{
    switch (e)
    {
#define XX(num, string)                                                        \
    case HPE_##num:                                                            \
        return string;

        HTTP_ERRNO_MAP(XX)

#undef XX
    }
    return "uknown http parser error";
}

} // namespace http
