#pragma once

#include "../http/http_parser_type.h"
#include "../http/http_version.h"

////////////////////////////////////////////////////////////////////////////////

struct hdr_info
{
    std::string data_;
    uint16_t cnt_on_begin_ = 0;
    uint16_t cnt_on_data_  = 0;
    uint16_t cnt_on_end_   = 0;

    void clear()
    {
        data_.clear();
        cnt_on_begin_ = 0;
        cnt_on_data_  = 0;
        cnt_on_end_   = 0;
    }

    friend bool operator<(const hdr_info& lhs, const hdr_info& rhs)
    {
        return lhs.data_ < rhs.data_;
    }
};

////////////////////////////////////////////////////////////////////////////////
// A helper class for testing the correctness of the http parser
template <typename ParserType>
class parser_notified
{
protected:
    hdr_info curr_hdr_key_;
    hdr_info curr_hdr_val_;

public:
    // Too lazy to make this data private and provide public functions
    boost::container::flat_map<hdr_info, hdr_info> hdrs_;
    http::http_version version_{0, 0};

    uint16_t cnt_on_msg_begin_ = 0;
    uint16_t cnt_on_version_   = 0;
    uint16_t cnt_on_hdrs_end_  = 0;
    uint16_t cnt_on_msg_end_   = 0;

    int return_res_ = 0;

public:
    int on_msg_begin(ParserType);
    int on_http_version(http::http_version v, ParserType);
    int on_hdr_key_begin(ParserType);
    int on_hdr_key_data(const char* d, size_t s, ParserType);
    int on_hdr_key_end(ParserType);
    int on_hdr_val_begin(ParserType);
    int on_hdr_val_data(const char* d, size_t s, ParserType);
    int on_hdr_val_end(ParserType);
    int on_hdrs_end(ParserType);
    int on_msg_end(ParserType);

    // Returns empty hdr_info if not found
    std::pair<hdr_info, hdr_info> find_hdr_info(const char* hdr) const;

protected:
    void reset_impl();
};

////////////////////////////////////////////////////////////////////////////////
// A helper class for testing the correctness of the http request parser
struct req_parser_notified : public parser_notified<http::req_parser>
{
    // Too lazy to make this data private and provide public functions
    std::string url_;
    http_method method_ = (http_method)-1;

    uint16_t cnt_on_url_begin_ = 0;
    uint16_t cnt_on_url_data_  = 0;
    uint16_t cnt_on_url_end_   = 0;
    uint16_t cnt_on_method_    = 0;

    req_parser_notified();

    int on_method(http_method);
    int on_url_begin();
    int on_url_data(const char* d, size_t s);
    int on_url_end();

    void reset();
};

////////////////////////////////////////////////////////////////////////////////
// A helper class for testing the correctness of the http response parser
struct resp_parser_notified : public parser_notified<http::resp_parser>
{
    http_status status_ = (http_status)-1;

    uint16_t cnt_on_status_              = 0;
    uint16_t cnt_on_trailing_hdrs_begin_ = 0;
    uint16_t cnt_on_trailing_hdrs_end_   = 0;

    resp_parser_notified();

    int on_status_code(http_status m);
    int on_trailing_hdrs_begin();
    int on_trailing_hdrs_end();

    void reset();
};
