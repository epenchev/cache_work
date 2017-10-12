#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../http/http_msg_parser.h"
#include "parser_notified.h"

using namespace http;
using req_parser_t  = http_msg_parser<req_parser_notified, req_parser>;
using resp_parser_t = http_msg_parser<resp_parser_notified, resp_parser>;

// Note that in these tests we don't try to test the internal (nodejs) parser
// functionality. We test here our parser wrapper and it's state machine
// functionality.
BOOST_AUTO_TEST_SUITE(http_msg_parser_tests)

BOOST_AUTO_TEST_CASE(req_no_hdrs)
{
    const string_view_t req{"GET /test/demo_form.asp HTTP/1.1\r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_GET;
    const http_version exp_ver{1, 1};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(bytes, req.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);
}

BOOST_AUTO_TEST_CASE(req_http_0_9)
{
    const string_view_t req{"GET /index.html\r\n\r\n"};
    const string_view_t exp_url{"/index.html"};
    const http_method exp_method = HTTP_GET;
    const http_version exp_ver{0, 9};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(bytes, req.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);
}

BOOST_AUTO_TEST_CASE(req_with_hdrs_no_body)
{
    const string_view_t req{"POST /test/demo_form.asp HTTP/1.0\r\n"
                            "Host: w3schools.com\r\n"
                            "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_POST;
    const http_version exp_ver{1, 0};
    const string_view_t exp_host_hdr{"w3schools.com"};
    const string_view_t exp_user_agent{"HTTPTool/1.0"};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(bytes, req.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("User-Agent");
        BOOST_CHECK_EQUAL(info.second.data_, exp_user_agent);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(move_construction)
{
    const string_view_t req1{"POST /test/demo_form.asp HTTP/1.0\r\n"};
    const string_view_t req2{"Host: w3schools.com\r\n"
                             "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_POST;
    const http_version exp_ver{1, 0};
    const string_view_t exp_host_hdr{"w3schools.com"};
    const string_view_t exp_user_agent{"HTTPTool/1.0"};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes1 = p.execute((const bytes8_t*)req1.data(), req1.size());
    BOOST_CHECK_EQUAL(bytes1, req1.size());

    req_parser_t p2(std::move(p));
    p.set_notified(pn);

    // The first parser must have been reset
    BOOST_CHECK_EQUAL(p.hdr_bytes(), 0);
    BOOST_CHECK_EQUAL(p.msg_bytes(), 0);
    // The first parser must fail if get called with the remaining request
    const auto bytes = p.execute((const bytes8_t*)req2.data(), req2.size());
    // The internal parser consumes 1 byte before it realizes that
    // there is an error.
    BOOST_CHECK_EQUAL(bytes, 1);
    BOOST_CHECK_NE(p.get_error_code(), HPE_OK);

    const auto bytes2 = p2.execute((const bytes8_t*)req2.data(), req2.size());
    BOOST_CHECK_EQUAL(bytes2, req2.size());
    // The 3 sentinel events must be called always only once
    // The internal parser calls on_msg_begin once before it realizes
    // that there is an error.
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 2);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(p2.hdr_bytes(), bytes1 + bytes2);
    BOOST_CHECK_EQUAL(p2.hdr_bytes(), req1.size() + req2.size());
    BOOST_CHECK_EQUAL(p2.msg_bytes(), req1.size() + req2.size());

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p2.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p2.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("User-Agent");
        BOOST_CHECK_EQUAL(info.second.data_, exp_user_agent);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    const string_view_t req1{"POST /test/demo_form.asp HTTP/1.0\r\n"};
    const string_view_t req2{"Host: w3schools.com\r\n"
                             "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_POST;
    const http_version exp_ver{1, 0};
    const string_view_t exp_host_hdr{"w3schools.com"};
    const string_view_t exp_user_agent{"HTTPTool/1.0"};

    req_parser_notified pn;
    req_parser_t p(pn);
    req_parser_t p2(pn);

    const auto bytes1 = p.execute((const bytes8_t*)req1.data(), req1.size());
    BOOST_CHECK_EQUAL(bytes1, req1.size());

    p2 = std::move(p);
    p.set_notified(pn);

    // The first parser must have been reset
    BOOST_CHECK_EQUAL(p.hdr_bytes(), 0);
    BOOST_CHECK_EQUAL(p.msg_bytes(), 0);
    // The first parser must fail if get called with the remaining request
    const auto bytes = p.execute((const bytes8_t*)req2.data(), req2.size());
    // The internal parser consumes 1 byte before it realizes that
    // there is an error.
    BOOST_CHECK_EQUAL(bytes, 1);
    BOOST_CHECK_NE(p.get_error_code(), HPE_OK);

    const auto bytes2 = p2.execute((const bytes8_t*)req2.data(), req2.size());
    BOOST_CHECK_EQUAL(bytes2, req2.size());
    // The 3 sentinel events must be called always only once
    // The internal parser calls on_msg_begin once before it realizes
    // that there is an error.
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 2);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(p2.hdr_bytes(), bytes1 + bytes2);
    BOOST_CHECK_EQUAL(p2.hdr_bytes(), req1.size() + req2.size());
    BOOST_CHECK_EQUAL(p2.msg_bytes(), req1.size() + req2.size());

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p2.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p2.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("User-Agent");
        BOOST_CHECK_EQUAL(info.second.data_, exp_user_agent);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(req_with_hdrs_no_body_empty_hdr_values)
{
    const string_view_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                            "Host:\r\n"
                            "User-Agent: \r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_GET;
    const http_version exp_ver{1, 1};
    const string_view_t exp_host_hdr{""};
    const string_view_t exp_user_agent{""};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(bytes, req.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("User-Agent");
        BOOST_CHECK_EQUAL(info.second.data_, exp_user_agent);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(req_with_hdrs_with_body)
{
    const string_view_t req{"POST /test/demo_form.asp HTTP/1.0\r\n"
                            "Host: w3schools.com\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shits</html>"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_POST;
    const http_version exp_ver{1, 0};
    const string_view_t exp_host_hdr{"w3schools.com"};
    const string_view_t exp_type_hdr{"text/html"};
    const string_view_t exp_length_hdr{"18"};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(bytes, req.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("Content-Type");
        BOOST_CHECK_EQUAL(info.second.data_, exp_type_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("Content-Length");
        BOOST_CHECK_EQUAL(info.second.data_, exp_length_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(req_with_hdrs_with_body_all_in_parts)
{
    // The full request
    const string_view_t full_req{"POST /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Content-Length: 18\r\n\r\n"
                                 "<html>shits</html>"};
    const string_view_t req1{"PO"};
    // Two parts method, 3 parts URL
    const string_view_t req2{"ST /test/"};
    const string_view_t req3{"demo_form.a"};
    // Two parts http version
    const string_view_t req4{"sp HTTP/1."};
    const string_view_t req5{"1\r\nHost"};
    // 3 parts host value
    const string_view_t req6{": w3"};
    const string_view_t req7{"schools.co"};
    const string_view_t req8{"m\r\n"};
    // Two parts content-type key
    const string_view_t req9{"Content-"};
    // Two parts content-type value
    const string_view_t req10{"Type: tex"};
    // Two parts content-length value
    const string_view_t req11{"t/html\r\nContent-Length: 1"};
    // Two parts body
    const string_view_t req12{"8\r\n\r\n<html>shi"};
    const string_view_t req13{"ts</html>"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_POST;
    const http_version exp_ver{1, 1};
    const string_view_t exp_host_hdr{"w3schools.com"};
    const string_view_t exp_type_hdr{"text/html"};
    const string_view_t exp_length_hdr{"18"};
    const uint16_t exp_cnt_on_url_data       = 3;
    const uint16_t exp_cnt_on_host_key_data  = 2;
    const uint16_t exp_cnt_on_host_val_data  = 3;
    const uint16_t exp_cnt_on_ctype_key_data = 2;
    const uint16_t exp_cnt_on_ctype_val_data = 2;
    const uint16_t exp_cnt_on_clen_key_data  = 1;
    const uint16_t exp_cnt_on_clen_val_data  = 2;

    const string_view_t reqs[] = {req1, req2, req3,  req4,  req5,  req6, req7,
                                  req8, req9, req10, req11, req12, req13};
    // This is assert not a unit test mechanics.
    // Just checks that the request pieces are OK.
    BOOST_ASSERT(full_req.size() == std::accumulate(std::begin(reqs),
                                                    std::end(reqs), 0U,
                                                    [](size_t l, const auto& r)
                                                    {
                                                        return l + r.size();
                                                    }));

    req_parser_notified pn;
    req_parser_t p(pn);

    for (auto r : reqs)
    {
        const auto bytes = p.execute((const bytes8_t*)r.data(), r.size());
        BOOST_CHECK_EQUAL(bytes, r.size());
    }

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, exp_cnt_on_url_data);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, exp_cnt_on_host_key_data);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, exp_cnt_on_host_val_data);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("Content-Type");
        BOOST_CHECK_EQUAL(info.second.data_, exp_type_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, exp_cnt_on_ctype_key_data);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, exp_cnt_on_ctype_val_data);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("Content-Length");
        BOOST_CHECK_EQUAL(info.second.data_, exp_length_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, exp_cnt_on_clen_key_data);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, exp_cnt_on_clen_val_data);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(req_broken)
{
    const string_view_t req{"GET /test/demo_form.asp OMG_PROTO/1.0\r\n"
                            "Host: w3schools.com\r\n"
                            "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_GET;
    const http_version exp_ver{0, 0};
    const string_view_t exp_host_hdr;
    const string_view_t exp_user_agent;

    // When the broken protocol is detected the parser must enter in error
    // state and nothing else should happen.

    req_parser_notified pn;
    req_parser_t p(pn);

    auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(
        bytes - 1,
        ("GET " + std::string(exp_url.data(), exp_url.size())).size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 0);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 0);

    // The parser shall be in error state
    BOOST_CHECK_NE(p.get_error_code(), HPE_OK);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 0); // We won't receive URL end
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 0); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);

    // No headers should be received at all
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 0);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 0);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 0);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 0);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 0);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 0);
    }
    {
        const auto info = pn.find_hdr_info("User-Agent");
        BOOST_CHECK_EQUAL(info.second.data_, exp_user_agent);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 0);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 0);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 0);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 0);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 0);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 0);
    }

    // The parser is in error state and subsequent calls won't do anything
    bytes = p.execute((const bytes8_t*)req.data(), req.size());
    BOOST_CHECK_EQUAL(bytes, 0);
}

BOOST_AUTO_TEST_CASE(req_break_and_reset)
{
    const string_view_t req1{"POST /test/demo_form.asp HTTP/1.0\r\n"};
    const string_view_t req2{"Host: w3schools.com\r\n"
                             "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view_t exp_url{"/test/demo_form.asp"};
    const http_method exp_method = HTTP_POST;
    const http_version exp_ver{1, 0};
    const string_view_t exp_host_hdr{"w3schools.com"};
    const string_view_t exp_user_agent{"HTTPTool/1.0"};

    req_parser_notified pn;
    req_parser_t p(pn);

    p.execute((const bytes8_t*)req1.data(), req1.size());
    BOOST_CHECK_EQUAL(p.get_error_code(), HPE_OK); // Parser is now OK
    pn.return_res_ = -1;
    // The parser enter the error state when our callback returns false.
    p.execute((const bytes8_t*)req2.data(), req2.size());
    BOOST_CHECK_NE(p.get_error_code(), HPE_OK); // Parser is now in error
    // The parser is in error state and subsequent calls won't do anything
    const auto bytes = p.execute((const bytes8_t*)req1.data(), req1.size());
    BOOST_CHECK_NE(p.get_error_code(), HPE_OK); // Parser is now in error
    BOOST_CHECK_EQUAL(bytes, 0);

    // Reseting the parser allows it to be reused
    p.reset();
    pn.reset();
    BOOST_CHECK_EQUAL(p.get_error_code(), HPE_OK);

    const auto bytes1 = p.execute((const bytes8_t*)req1.data(), req1.size());
    const auto bytes2 = p.execute((const bytes8_t*)req2.data(), req2.size());

    BOOST_CHECK_EQUAL(bytes1, req1.size());
    BOOST_CHECK_EQUAL(bytes2, req2.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.method_, exp_method);
    BOOST_CHECK_EQUAL(pn.cnt_on_method_, 1); // The method must be set once
    BOOST_CHECK_EQUAL(p.get_method(), exp_method);
    BOOST_CHECK_EQUAL(pn.url_, exp_url);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_url_data_, 1); // The URL must be set at once
    BOOST_CHECK_EQUAL(pn.cnt_on_url_end_, 1);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    BOOST_CHECK_EQUAL(pn.cnt_on_version_, 1); // The version must be set once
    BOOST_CHECK_EQUAL(p.get_http_version(), exp_ver);

    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Host");
        BOOST_CHECK_EQUAL(info.second.data_, exp_host_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("User-Agent");
        BOOST_CHECK_EQUAL(info.second.data_, exp_user_agent);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(req_hdr_and_msg_bytes_when_pieces)
{
    // The full request
    const string_view_t full_req{"POST /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Content-Length: 18\r\n\r\n"
                                 "<html>shits</html>"};
    const string_view_t req1{"PO"};
    // Two parts method, 3 parts URL
    const string_view_t req2{"ST /test/"};
    const string_view_t req3{"demo_form.a"};
    // Two parts http version
    const string_view_t req4{"sp HTTP/1."};
    const string_view_t req5{"1\r\nHost"};
    // 3 parts host value
    const string_view_t req6{": w3"};
    const string_view_t req7{"schools.co"};
    const string_view_t req8{"m\r\n"};
    // Two parts content-type key
    const string_view_t req9{"Content-"};
    // Two parts content-type value
    const string_view_t req10{"Type: tex"};
    // Two parts content-length value
    const string_view_t req11{"t/html\r\nContent-Length: 1"};
    // Two parts body
    const string_view_t req12{"8\r\n\r\n<html>shi"};
    const string_view_t req13{"ts</html>"};

    auto exp_hdr_len = full_req.find("\r\n\r\n");
    BOOST_ASSERT(exp_hdr_len != string_view_t::npos);
    exp_hdr_len += 4;

    const string_view_t reqs[] = {req1, req2, req3,  req4,  req5,  req6, req7,
                                  req8, req9, req10, req11, req12, req13};
    // This is assert not a unit test mechanics.
    // Just checks that the request pieces are OK.
    BOOST_ASSERT(full_req.size() == std::accumulate(std::begin(reqs),
                                                    std::end(reqs), 0U,
                                                    [](size_t l, const auto& r)
                                                    {
                                                        return l + r.size();
                                                    }));

    req_parser_notified pn;
    req_parser_t p(pn);

    for (auto r : reqs)
    {
        const auto bytes = p.execute((const bytes8_t*)r.data(), r.size());
        BOOST_CHECK_EQUAL(bytes, r.size());
    }

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(p.hdr_bytes(), exp_hdr_len);
    BOOST_CHECK_EQUAL(p.msg_bytes(), full_req.size());
}

BOOST_AUTO_TEST_CASE(req_hdr_and_msg_bytes_when_no_hdrs)
{
    const string_view_t req{"GET /test/demo_form.asp HTTP/1.1\r\n\r\n"};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    BOOST_CHECK_EQUAL(bytes, req.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(p.hdr_bytes(), req.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), req.size());
}

BOOST_AUTO_TEST_CASE(req_hdr_and_msg_bytes_when_hdrs)
{
    // The full request
    const string_view_t req{"POST /test/demo_form.asp HTTP/1.1\r\n"
                            "Host: w3schools.com\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Best: 18\r\n\r\n"};

    req_parser_notified pn;
    req_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)req.data(), req.size());

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(bytes, req.size());
    BOOST_CHECK_EQUAL(p.hdr_bytes(), req.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), req.size());
}

////////////////////////////////////////////////////////////////////////////////
// Response test cases
// There are fewer response test cases because we have only two differences
// between handling request and responses in our wrapper.
// We need to test only them.
//
// 1. The first one is when we have single response line without body
BOOST_AUTO_TEST_CASE(resp_no_hdrs_no_body)
{
    const string_view_t resp{"HTTP/1.1 404 Not Found\r\n\r\n"};
    const http_status exp_status = HTTP_STATUS_NOT_FOUND;
    const http_version exp_ver{1, 1};

    resp_parser_notified pn;
    resp_parser_t p(pn);

    // Tell the parser explicitly to skip the body,
    // because there is no content-length
    pn.return_res_   = http::res_skip_body;
    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    BOOST_CHECK_EQUAL(bytes, resp.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.status_, exp_status);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
}

BOOST_AUTO_TEST_CASE(resp_no_hdrs_no_body_with_rnrn)
{
    const string_view_t resp{"\r\n\r\nHTTP/1.1 404 Not Found\r\n\r\n"};
    const http_status exp_status = HTTP_STATUS_NOT_FOUND;
    const http_version exp_ver{1, 1};

    resp_parser_notified pn;
    resp_parser_t p(pn);

    // Tell the parser explicitly to skip the body,
    // because there is no content-length
    pn.return_res_   = http::res_skip_body;
    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    BOOST_CHECK_EQUAL(bytes, resp.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.status_, exp_status);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
}

// 2. This is the second and last difference how our parser handles responses
// vs requests. The difference is when the headers start after the response
// line (after on_msg_begin).
BOOST_AUTO_TEST_CASE(resp_with_headers_only)
{
    const string_view_t resp{"HTTP/1.0 503 Service Unavailable\r\n"
                             "Server: nginx\r\n"
                             "Date: Fri, 04 Nov 2016 15:03:53 GMT\r\n"
                             "Connection: keep-alive\r\n\r\n"};
    const http_status exp_status = HTTP_STATUS_SERVICE_UNAVAILABLE;
    const http_version exp_ver{1, 0};
    const string_view_t exp_server_hdr{"nginx"};
    const string_view_t exp_date_hdr{"Fri, 04 Nov 2016 15:03:53 GMT"};
    const string_view_t exp_conn_hdr{"keep-alive"};
    const uint16_t exp_cnt_on_server_key_data = 1;
    const uint16_t exp_cnt_on_server_val_data = 1;
    const uint16_t exp_cnt_on_date_key_data   = 1;
    const uint16_t exp_cnt_on_date_val_data   = 1;
    const uint16_t exp_cnt_on_conn_key_data   = 1;
    const uint16_t exp_cnt_on_conn_val_data   = 1;

    resp_parser_notified pn;
    resp_parser_t p(pn);

    // Tell the parser explicitly to skip the body,
    // because there is no content-length
    pn.return_res_   = http::res_skip_body;
    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    BOOST_CHECK_EQUAL(bytes, resp.size());
    BOOST_CHECK_EQUAL(p.hdr_bytes(), resp.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), resp.size());

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    BOOST_CHECK_EQUAL(pn.status_, exp_status);
    BOOST_CHECK_EQUAL(pn.version_, exp_ver);
    //
    // Now check that the headers are parsed in a correct way
    {
        const auto info = pn.find_hdr_info("Server");
        BOOST_CHECK_EQUAL(info.second.data_, exp_server_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, exp_cnt_on_server_key_data);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, exp_cnt_on_server_val_data);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("Date");
        BOOST_CHECK_EQUAL(info.second.data_, exp_date_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, exp_cnt_on_date_key_data);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, exp_cnt_on_date_val_data);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
    {
        const auto info = pn.find_hdr_info("Connection");
        BOOST_CHECK_EQUAL(info.second.data_, exp_conn_hdr);
        // The header notifications must have been received at once
        BOOST_CHECK_EQUAL(info.first.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.first.cnt_on_data_, exp_cnt_on_conn_key_data);
        BOOST_CHECK_EQUAL(info.first.cnt_on_end_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_begin_, 1);
        BOOST_CHECK_EQUAL(info.second.cnt_on_data_, exp_cnt_on_conn_val_data);
        BOOST_CHECK_EQUAL(info.second.cnt_on_end_, 1);
    }
}

BOOST_AUTO_TEST_CASE(resp_hdr_and_msg_bytes_when_pieces)
{
    // The full response
    const string_view_t full_resp{"HTTP/1.0 200 OK\r\n"
                                  "Host: w3schools.com\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: 18\r\n\r\n"
                                  "<html>shits</html>"};
    const string_view_t resp1{"HTT"};
    // Two parts method, 3 parts URL
    const string_view_t resp2{"P/1.0 20"};
    const string_view_t resp3{"0 "};
    // Two parts http version
    const string_view_t resp4{"OK"};
    const string_view_t resp5{"\r\nHost"};
    // 3 parts host value
    const string_view_t resp6{": w3"};
    const string_view_t resp7{"schools.co"};
    const string_view_t resp8{"m\r\n"};
    // Two parts content-type key
    const string_view_t resp9{"Content-"};
    // Two parts content-type value
    const string_view_t resp10{"Type: tex"};
    // Two parts content-length value
    const string_view_t resp11{"t/html\r\nContent-Length: 1"};
    // Two parts body
    const string_view_t resp12{"8\r\n\r\n<html>shi"};
    const string_view_t resp13{"ts</html>"};

    auto exp_hdr_len = full_resp.find("\r\n\r\n");
    BOOST_ASSERT(exp_hdr_len != string_view_t::npos);
    exp_hdr_len += 4;

    const string_view_t resps[] = {resp1,  resp2,  resp3, resp4, resp5,
                                   resp6,  resp7,  resp8, resp9, resp10,
                                   resp11, resp12, resp13};
    // This is assert not a unit test mechanics.
    // Just checks that the respuest pieces are OK.
    BOOST_ASSERT(full_resp.size() == std::accumulate(std::begin(resps),
                                                     std::end(resps), 0U,
                                                     [](size_t l, const auto& r)
                                                     {
                                                         return l + r.size();
                                                     }));

    resp_parser_notified pn;
    resp_parser_t p(pn);

    for (auto r : resps)
    {
        const auto bytes = p.execute((const bytes8_t*)r.data(), r.size());
        BOOST_CHECK_EQUAL(bytes, r.size());
    }

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(p.hdr_bytes(), exp_hdr_len);
    BOOST_CHECK_EQUAL(p.msg_bytes(), full_resp.size());
}

BOOST_AUTO_TEST_CASE(resp_hdr_and_msg_bytes_when_no_hdrs)
{
    const string_view_t resp{"HTTP/1.1 404 Not Found\r\n\r\n"};

    resp_parser_notified pn;
    resp_parser_t p(pn);

    // Tell the parser explicitly to skip the body,
    // because there is no content-length
    pn.return_res_   = http::res_skip_body;
    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    BOOST_CHECK_EQUAL(bytes, resp.size());
    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(p.hdr_bytes(), resp.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), resp.size());
}

BOOST_AUTO_TEST_CASE(resp_hdr_and_msg_bytes_when_hdrs)
{
    const string_view_t resp{"HTTP/1.1 500 Server Error\r\n"
                             "Most: w3schools.com\r\n"
                             "Content-No: naida\r\n"
                             "Content-Best: 180\r\n\r\n"};

    resp_parser_notified pn;
    resp_parser_t p(pn);

    // Tell the parser explicitly to skip the body,
    // because there is no content-length
    pn.return_res_   = http::res_skip_body;
    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(bytes, resp.size());
    BOOST_CHECK_EQUAL(p.hdr_bytes(), resp.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), resp.size());
}

BOOST_AUTO_TEST_CASE(resp_hdr_and_msg_bytes_when_chunked_data_and_no_footers)
{
    const const_string_t resp{"HTTP/1.1 200 OK\r\n"
                              "Date: Fri, 31 Dec 1999 23:59:59 GMT\r\n"
                              "Content-Type: text/plain\r\n"
                              "Transfer-Encoding: chunked\r\n\r\n"
                              "1a; ignore-stuff-here\r\n"
                              "abcdefghijklmnopqrstuvwxyz\r\n"
                              "10\r\n"
                              "1234567890abcdef\r\n"
                              "0\r\n\r\n"};

    resp_parser_notified pn;
    resp_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(bytes, resp.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), resp.size());
}

BOOST_AUTO_TEST_CASE(resp_hdr_and_msg_bytes_when_chunked_data_and_footers)
{
    const const_string_t resp{"HTTP/1.1 200 OK\r\n"
                              "Date: Fri, 31 Dec 1999 23:59:59 GMT\r\n"
                              "Content-Type: text/plain\r\n"
                              "Transfer-Encoding: chunked\r\n\r\n"
                              "1a; ignore-stuff-here\r\n"
                              "abcdefghijklmnopqrstuvwxyz\r\n"
                              "10\r\n"
                              "1234567890abcdef\r\n"
                              "0\r\n"
                              "some-footer: some-value\r\n"
                              "another-footer: another-value\r\n\r\n"};

    resp_parser_notified pn;
    resp_parser_t p(pn);

    const auto bytes = p.execute((const bytes8_t*)resp.data(), resp.size());

    // The 3 sentinel events must be called always only once
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_trailing_hdrs_begin_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_trailing_hdrs_end_, 1);
    BOOST_CHECK_EQUAL(pn.cnt_on_msg_end_, 1);

    // Check the counted header and message bytes
    BOOST_CHECK_EQUAL(bytes, resp.size());
    BOOST_CHECK_EQUAL(p.msg_bytes(), resp.size());
}

BOOST_AUTO_TEST_SUITE_END()
