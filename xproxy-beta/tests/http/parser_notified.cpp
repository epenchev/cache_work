#include "precompiled.h"
#include "parser_notified.h"

template <typename PT>
int parser_notified<PT>::on_msg_begin(PT)
{
    ++cnt_on_msg_begin_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_http_version(http::http_version v, PT)
{
    ++cnt_on_version_;
    version_ = v;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdr_key_begin(PT)
{
    ++curr_hdr_key_.cnt_on_begin_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdr_key_data(const char* d, size_t s, PT)
{
    ++curr_hdr_key_.cnt_on_data_;
    curr_hdr_key_.data_.append(d, s);
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdr_key_end(PT)
{
    ++curr_hdr_key_.cnt_on_end_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdr_val_begin(PT)
{
    ++curr_hdr_val_.cnt_on_begin_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdr_val_data(const char* d, size_t s, PT)
{
    ++curr_hdr_val_.cnt_on_data_;
    curr_hdr_val_.data_.append(d, s);
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdr_val_end(PT)
{
    ++curr_hdr_val_.cnt_on_end_;
    hdrs_.emplace(std::move(curr_hdr_key_), std::move(curr_hdr_val_));
    curr_hdr_key_.clear();
    curr_hdr_val_.clear();
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
int parser_notified<PT>::on_hdrs_end(PT)
{
    ++cnt_on_hdrs_end_;
    return return_res_; // Return skip body here
}

template <typename PT>
int parser_notified<PT>::on_msg_end(PT)
{
    ++cnt_on_msg_end_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

template <typename PT>
std::pair<hdr_info, hdr_info>
parser_notified<PT>::find_hdr_info(const char* hdr) const
{
    hdr_info tmp;
    tmp.data_ = hdr;
    auto it = hdrs_.find(tmp);
    return (it != hdrs_.cend()) ? *it : std::pair<hdr_info, hdr_info>{};
}

template <typename PT>
void parser_notified<PT>::reset_impl()
{
    hdrs_.clear();
    version_ = http::http_version{0, 0};

    cnt_on_msg_begin_ = 0;
    cnt_on_version_   = 0;
    cnt_on_hdrs_end_  = 0;
    cnt_on_msg_end_   = 0;

    return_res_ = 0;
}

template class parser_notified<http::req_parser>;
template class parser_notified<http::resp_parser>;

////////////////////////////////////////////////////////////////////////////////

req_parser_notified::req_parser_notified()
{
}

int req_parser_notified::on_method(http_method m)
{
    ++cnt_on_method_;
    method_ = m;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

int req_parser_notified::on_url_begin()
{
    ++cnt_on_url_begin_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

int req_parser_notified::on_url_data(const char* d, size_t s)
{
    ++cnt_on_url_data_;
    url_.append(d, s);
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

int req_parser_notified::on_url_end()
{
    ++cnt_on_url_end_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

void req_parser_notified::reset()
{
    reset_impl();
    url_.clear();
    method_           = (http_method)-1;
    cnt_on_url_begin_ = 0;
    cnt_on_url_data_  = 0;
    cnt_on_url_end_   = 0;
    cnt_on_method_    = 0;
}

////////////////////////////////////////////////////////////////////////////////

resp_parser_notified::resp_parser_notified()
{
}

int resp_parser_notified::on_status_code(http_status m)
{
    ++cnt_on_status_;
    status_ = m;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

int resp_parser_notified::on_trailing_hdrs_begin()
{
    ++cnt_on_trailing_hdrs_begin_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

int resp_parser_notified::on_trailing_hdrs_end()
{
    ++cnt_on_trailing_hdrs_end_;
    return return_res_ >= 0 ? 0 : -1; // Don't return skip body here
}

void resp_parser_notified::reset()
{
    reset_impl();
    cnt_on_status_ = 0;
}
