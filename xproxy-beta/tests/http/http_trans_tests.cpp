#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../http/http_trans.h"
#include "../cache/cache_key.h"

namespace cache
{
bool operator==(const cache_key::rng& lhs, const cache_key::rng& rhs) noexcept
{
    return lhs.beg_ == rhs.beg_ && lhs.end_ == rhs.end_;
}
} // namespace cache

using http::http_trans;

BOOST_AUTO_TEST_SUITE(http_trans_tests)

////////////////////////////////////////////////////////////////////////////////
// Request handling tests

BOOST_AUTO_TEST_CASE(construct)
{
    id_tag tag;
    http_trans trans(tag);
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
}

BOOST_AUTO_TEST_CASE(parse_full_req_no_hdrs)
{
    const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    // The transaction is keep alive only if it's confirmed by the
    // request and the response. We still don't have response in this case.
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
}

BOOST_AUTO_TEST_CASE(http_tunnel_non_get_req_with_hdrs)
{
    const_string_t req{"POST /test/best HTTP/1.0\r\n"
                       "Host: w3schools.com\r\n"
                       "User-Agent: HTTPTool/1.0\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
}

BOOST_AUTO_TEST_CASE(unsupported_on_get_req_http_0_9)
{
    const_string_t req{"GET /test/best"};
    const_string_t req2{"\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);
    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::proceed);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);

    ret = trans.on_req_data((const uint8_t*)req2.data(), req2.size());
    BOOST_CHECK(ret.res_ == http_trans::res::unsupported);
}

// Actual case seen in practice as strange as it sound
BOOST_AUTO_TEST_CASE(http_tunnel_get_req_with_body)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Content-Length: 18\r\n\r\n"
                                 "<html>shits</html>"};
    constexpr auto req_content_len = 18;
    constexpr auto req_hdrs_len    = req.size() - req_content_len;

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_REQUIRE(trans.req_content_len());
    BOOST_CHECK_EQUAL(*trans.req_content_len(), req_content_len);
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
}

BOOST_AUTO_TEST_CASE(do_not_parse_req_content_len_in_http_tunnel_mode)
{
    constexpr const_string_t req{"POST /test/best_bank HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Content-Length: 18\r\n\r\n"
                                 "<html>shits</html>"};
    constexpr auto req_content_len = 18;
    constexpr auto req_hdrs_len    = req.size() - req_content_len;

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    // The POST method puts the transaction in HTTP tunnel mode
    BOOST_CHECK(trans.in_http_tunnel());
    // and the 'Content-Length' field is not collected in such mode,
    // because it's used only for decision if we need to go to HTTP tunnel mode.
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
}

BOOST_AUTO_TEST_CASE(unsupported_on_connect_req)
{
    constexpr const_string_t req{"CONNECT server.example.com:80 HTTP/1.1\r\n"
                                 "Host: server.example.com:80\r\n\r\n"};
    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::unsupported);
    BOOST_CHECK_LE(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
}

BOOST_AUTO_TEST_CASE(unsupported_on_upgrade_req)
{
    constexpr const_string_t req{
        "GET http://example.bank.com/acct_stat.html?749394889300 HTTP/1.1\r\n"
        "Host: example.bank.com\r\n"
        "Upgrade: TLS/1.0\r\n"
        "Connection: Upgrade\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::unsupported);
    BOOST_CHECK_LE(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
}

BOOST_AUTO_TEST_CASE(unsupported_on_authorization_req)
{
    constexpr const_string_t req{
        "GET /securefiles/ HTTP/1.1\r\n"
        "Host: www.httpwatch.com\r\n"
        "Authorization: Basic aHR0cHdhdGNoOmY=\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::unsupported);
    BOOST_CHECK_LE(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
}

BOOST_AUTO_TEST_CASE(
    http_tunnel_on_non_get_req_and_then_unsupported_on_authorization_req)
{
    constexpr const_string_t req{"POST /securefiles/api HTTP/1.1\r\n"};
    constexpr const_string_t req2{
        "Host: www.httpwatch.com\r\n"
        "Authorization: Basic aHR0cHdhdGNoOmY=\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);
    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::proceed);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    // The POST method has put the transaction in HTTP tunnel mode
    BOOST_CHECK(trans.in_http_tunnel());

    ret = trans.on_req_data((const uint8_t*)req2.data(), req2.size());

    // The 'Authorization' header put the transaction in 'unsupported' mode
    BOOST_CHECK(ret.res_ == http_trans::res::unsupported);
    BOOST_CHECK_LE(ret.consumed_, req2.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
}

BOOST_AUTO_TEST_CASE(force_http_tunnel_on_unfinished_req)
{
    const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::proceed);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    // The transaction is keep alive only if it's confirmed by the
    // request and the response. We still don't have response in this case.
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.get_cache_key());
    // The currently parsed request header bytes are reported
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
    // Not in HTTP tunnel
    BOOST_CHECK(!trans.in_http_tunnel());
    trans.force_http_tunnel();
    // Now in HTTP tunnel
    BOOST_CHECK(trans.in_http_tunnel());
}

BOOST_AUTO_TEST_CASE(force_req_done_on_unfinished_req)
{
    const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::proceed);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(!trans.is_chunked());
    // The transaction is keep alive only if it's confirmed by the
    // request and the response. We still don't have response in this case.
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.get_cache_key());
    // The currently parsed request header bytes are reported
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
    // The request is not yet completed
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    // And we are not in HTTP tunnel
    BOOST_CHECK(!trans.in_http_tunnel());
    trans.on_req_end_of_stream();
    // Now the request is completed, but without headers
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    // And the transactions is now in HTTP tunnel mode
    BOOST_CHECK(trans.in_http_tunnel());
}

// We don't check different kind of garbage requests, because the error
// itself comes from the parser, we just need to confirm here that the
// transaction enters 'error' state in case of parser error.
BOOST_AUTO_TEST_CASE(error_on_garbage_req)
{
    const_string_t req{"PIRUET agagaegsbretaerasdfasdf\r\n"};

    id_tag tag;
    http_trans trans(tag);
    const auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());

    BOOST_CHECK(ret.res_ == http_trans::res::error);
    BOOST_CHECK_LE(ret.consumed_, req.size());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    // We are not in HTTP tunnel mode, we are in error state.
    BOOST_CHECK(!trans.in_http_tunnel());
}

// Again we don't try to test the parser behavior here, we test only
// the states and the bytes reported by the transaction. That's why
// we set the request to be not very fragmented.
BOOST_AUTO_TEST_CASE(correct_bytes_states_when_req_in_parts)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"};
    constexpr const_string_t req2{"Content-Type: text/html\r\n"
                                  "Content-Length: 18\r\n\r\n"};
    constexpr const_string_t req3{"<html>shits</html>"};
    constexpr auto req_content_len = 18;

    id_tag tag;
    http_trans trans(tag);
    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    // Not all headers are yet processed
    BOOST_CHECK(ret.res_ == http_trans::res::proceed);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());
    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());

    ret = trans.on_req_data((const uint8_t*)req2.data(), req2.size());
    // All headers are now processed
    BOOST_CHECK(ret.res_ == http_trans::res::proceed);
    BOOST_CHECK_EQUAL(ret.consumed_, req2.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_REQUIRE(trans.req_content_len());
    BOOST_CHECK_EQUAL(*trans.req_content_len(), req_content_len);
    // We go to HTTP tunnel because of the 'Content-Length' in GET request.
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size() + req2.size());
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size() + req2.size());

    ret = trans.on_req_data((const uint8_t*)req3.data(), req3.size());
    // All message is now processed
    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, req3.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_REQUIRE(trans.req_content_len());
    BOOST_CHECK_EQUAL(*trans.req_content_len(), req_content_len);
    // We remain in HTTP tunnel
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size() + req2.size());
    BOOST_CHECK_EQUAL(trans.req_bytes(),
                      req.size() + req2.size() + req3.size());
}

////////////////////////////////////////////////////////////////////////////////
// Response handling tests

BOOST_AUTO_TEST_CASE(parse_full_resp_no_hdrs)
{
    const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n\r\n"};
    const_string_t resp{"HTTP/1.1 404 Not Found\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_CHECK(ret.res_ == http_trans::res::complete);
    BOOST_CHECK_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_chunked());
    BOOST_CHECK(trans.is_keep_alive());
    // The 404 response provokes HTTP tunnel mode
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(!trans.get_cache_key());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size());
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
}

BOOST_AUTO_TEST_CASE(parse_full_resp_hdrs_body)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "User-Agent: HTTPTool/1.0\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: 18\r\n\r\n"
                                  "<html>shits</html>"};
    constexpr auto resp_content_len = 18;
    constexpr auto resp_hdrs_len    = resp.size() - resp_content_len;
    const string_view_t url{"w3schools.com/test/demo_form.asp"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE(ret.res_ == http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    BOOST_CHECK(!trans.is_chunked());
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key with URL and content-length only.
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(correct_bytes_states_when_response_in_parts)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "User-Agent: HTTPTool/1.0\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"};
    constexpr const_string_t resp2{"Content-Type: text/html\r\n"
                                   "Content-Length: 18\r\n\r\n"};
    constexpr const_string_t resp3{"<html>shits</html>"};
    constexpr auto resp_content_len = 18;
    constexpr auto resp_hdrs_len =
        resp.size() + resp2.size() + resp3.size() - resp_content_len;
    const string_view_t url{"w3schools.com/test/demo_form.asp"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE(ret.res_ == http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    // The headers are still not completed
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    BOOST_CHECK(!trans.get_cache_key());

    // Now the headers becomes completed and the cache key is present
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp2.size());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp_hdrs_len);
    // We must have valid cache key with URL and content-length only.
    auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);

    // Now the message becomes completed and the cache key is present
    ret = trans.on_resp_data((const uint8_t*)resp3.data(), resp3.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp3.size());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp_hdrs_len + resp3.size());
    // We must have valid cache key with URL and content-length only.
    ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(correct_url_when_host_in_req_line)
{
    constexpr const_string_t req{
        "GET http://www.w3schools.com/test/demo_form.asp HTTP/1.1\r\n"
        "Host: 192.168.56.101\r\n"
        "User-Agent: HTTPTool/1.0\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: 18\r\n\r\n"
                                  "<html>shits</html>"};
    constexpr auto resp_content_len = 18;
    constexpr auto resp_hdrs_len    = resp.size() - resp_content_len;
    const string_view_t url{"www.w3schools.com/test/demo_form.asp"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    BOOST_CHECK(!trans.is_chunked());
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key with URL and content-length only.
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(unsupported_on_invalid_resp_http_version)
{
    constexpr const_string_t req{
        "GET http://www.w3schools.com/test/demo_form.asp HTTP/1.1\r\n"
        "Host: 192.168.56.101\r\n"
        "User-Agent: HTTPTool/1.0\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.2 200 OK\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::unsupported);

    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
}

BOOST_AUTO_TEST_CASE(http_tunnel_on_status_not_200_nor_206)
{
    constexpr const_string_t req{
        "GET http://www.w3schools.com/test/demo_form.asp HTTP/1.1\r\n"
        "Host: 192.168.56.101\r\n"
        "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const_string_t resp{"HTTP/1.1 404 Not Found"};
    const_string_t resp2{"\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    // The headers and the message are still not completed
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.is_keep_alive());
    // The headers and the message are now completed
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp2.size());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(trans.is_keep_alive());
}

BOOST_AUTO_TEST_CASE(content_length_read_even_in_http_tunnel_mode)
{
    constexpr const_string_t req{
        "GET http://www.w3schools.com/test/demo_form.asp HTTP/1.1\r\n"
        "Host: 192.168.56.101\r\n"
        "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const_string_t resp{"HTTP/1.1 404 Not Found\r\n"
                        "Content-Length: 14\r\n\r\n"
                        "File Not Found"};
    constexpr auto content_len = 14;

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    // The headers and the message are still not completed
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    // We enter HTTP tunnel because of the status code
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), content_len);
}

BOOST_AUTO_TEST_CASE(
    http_tunnel_on_status_not_200_nor_206_and_unsupported_on_authenticate)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: 192.168.56.101\r\n"
                                 "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const_string_t resp{"HTTP/1.1 401 Access Denied\r\nWWW"};
    const_string_t resp2{"-Authenticate: Basic realm='My Server'\r\n"
                         "Content-Length: 0\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    // The headers and the message are still not completed
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(trans.in_http_tunnel());
    // Now it goes to unsupported mode, because of the authenticate
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::unsupported);
}

BOOST_AUTO_TEST_CASE(http_tunnel_on_transfer_encoding_chunked)
{
    const const_string_t req{"GET /path/file.html HTTP/1.1\r\n"
                             "Host: www.host1.com:80\r\n\r\n"};
    const const_string_t resp{"HTTP/1.1 200 OK\r\n"};
    const const_string_t resp2{"Date: Fri, 31 Dec 1999 23:59:59 GMT\r\n"
                               "Content-Type: text/plain\r\n"
                               "Transfer-Encoding: chunked\r\n\r\n"};
    const const_string_t resp3{"1a; ignore-stuff-here\r\n"
                               "abcdefghijklmnopqrstuvwxyz\r\n"
                               "10\r\n"
                               "1234567890abcdef\r\n"
                               "0\r\n"
                               "some-footer: some-value\r\n"
                               "another-footer: another-value\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    // The headers and the message are still not completed
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK(!trans.is_chunked());
    // Now it goes to http tunnel because of the chunked encoding
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(trans.is_chunked());
    // Now the transaction completes
    ret = trans.on_resp_data((const uint8_t*)resp3.data(), resp3.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::unsupported);
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK(trans.is_chunked());
}

BOOST_AUTO_TEST_CASE(http_tunnel_on_transfer_encoding_non_chunked)
{
    const const_string_t req{"GET /path/file.html HTTP/1.1\r\n"
                             "Host: www.host1.com:80\r\n\r\n"};
    const const_string_t resp{"HTTP/1.1 200 OK\r\n"};
    const const_string_t resp2{"Date: Fri, 31 Dec 1999 23:59:59 GMT\r\n"
                               "Content-Type: text/plain\r\n"
                               "Transfer-Encoding: gzip\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    // The headers and the message are still not completed
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.in_http_tunnel());
    // Now it goes to http tunnel because of the presence of 'Transfer-Encoding'
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_CHECK(trans.in_http_tunnel());
}

BOOST_AUTO_TEST_CASE(skip_body_non_chunked_no_content_length)
{
    const const_string_t req{
        "HEAD /bit/thinner-archives-vol-1.zip.torrent HTTP/1.1\r\n"
        "Host: www.legaltorrents.com \r\n\r\n"};
    const const_string_t resp{"HTTP/1.1 200 OK\r\n"
                              "Server: Apache\r\n"
                              "ETag: 1450013-6514-e905eec0\r\n"
                              "Connection: close\r\n"
                              "Content-Type: application/x-bittorrent\r\n\r\n"};
    const const_string_t body = "aaaa";

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());
    // We are already in HTTP tunnel because of the HEAD method
    BOOST_CHECK(trans.in_http_tunnel());
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_CHECK(trans.in_http_tunnel());
    // Now if we call the transaction with the received body,
    // we get an error.
    ret = trans.on_resp_data((const uint8_t*)body.data(), body.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::error);
}

BOOST_AUTO_TEST_CASE(get_content_encoding_when_no_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Server: Microsoft-IIS/8.0\r\n"
                                  "Content-Encoding: gzip\r\n"
                                  "Content-Length: 10\r\n\r\n"
                                  "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;
    const string_view_t url         = "www.httpwatch.com/httpgallery/compression/";
    const string_view_t encoding    = "gzip";

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK_EQUAL(ckey->content_encoding_, encoding);
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(get_no_content_encoding_when_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"};
    constexpr const_string_t resp2{"Server: Microsoft-IIS/8.0\r\n"
                                   "Content-Encoding: gzip\r\n"
                                   "Content-Length: 10\r\n\r\n"
                                   "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_REQUIRE(!trans.in_http_tunnel());
    // Now set the transaction forcefully in HTTP tunnel
    trans.force_http_tunnel();
    BOOST_REQUIRE(trans.in_http_tunnel());

    // Now the transaction should complete but without cache_key and
    // content-encoding
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp2.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    // However the content length must be present
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(),
                      resp.size() + resp2.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size() + resp2.size());
    BOOST_CHECK(!trans.get_cache_key());
}

BOOST_AUTO_TEST_CASE(get_content_md5_when_no_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Want-Digest: contentMD5\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Server: Microsoft-IIS/8.0\r\n"
                                  "Content-Encoding: gzip\r\n"
                                  "Content-MD5: Q2h1Y2sgSW51ZwDIAXR5IQ==\r\n"
                                  "Content-Length: 10\r\n\r\n"
                                  "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;
    const string_view_t url         = "www.httpwatch.com/httpgallery/compression/";
    const string_view_t encoding    = "gzip";
    const string_view_t content_md5 = "Q2h1Y2sgSW51ZwDIAXR5IQ==";

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK_EQUAL(ckey->content_encoding_, encoding);
    BOOST_CHECK_EQUAL(ckey->content_md5_, content_md5);
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(get_no_content_md5_when_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Want-Digest: contentMD5\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"};
    constexpr const_string_t resp2{"Server: Microsoft-IIS/8.0\r\n"
                                   "Content-Encoding: gzip\r\n"
                                   "Content-MD5: Q2h1Y2sgSW51ZwDIAXR5IQ==\r\n"
                                   "Content-Length: 10\r\n\r\n"
                                   "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_REQUIRE(!trans.in_http_tunnel());
    // Now set the transaction forcefully in HTTP tunnel
    trans.force_http_tunnel();
    BOOST_REQUIRE(trans.in_http_tunnel());

    // Now the transaction should complete but without cache_key and
    // content-encoding
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp2.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    // However the content length must be present
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(),
                      resp.size() + resp2.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size() + resp2.size());
    BOOST_CHECK(!trans.get_cache_key());
}

BOOST_AUTO_TEST_CASE(get_etag_when_no_http_tunnel)
{
    const const_string_t req{
        "GET /bit/thinner-archives-vol-1.zip.torrent HTTP/1.1\r\n"
        "Host: www.legaltorrents.com\r\n\r\n"};
    const const_string_t resp{
        "HTTP/1.1 200 OK\r\n"
        "Server: Apache\r\n"
        "ETag: 1450013-6514-e905eec0\r\n"
        "Connection: close\r\n"
        "Content-Length: 4\r\n"
        "Content-Type: application/x-bittorrent\r\n\r\naaaa"};
    constexpr auto resp_content_len = 4;
    const string_view_t url{
        "www.legaltorrents.com/bit/thinner-archives-vol-1.zip.torrent"};
    const string_view_t etag{"1450013-6514-e905eec0"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // There is connection: close i.e. not keep alive
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key with URL and content-length only.
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK_EQUAL(ckey->etag_, etag);
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(no_get_etag_when_http_tunnel)
{
    const const_string_t req{
        "GET /bit/thinner-archives-vol-1.zip.torrent HTTP/1.1\r\n"
        "Host: www.legaltorrents.com \r\n\r\n"};
    const const_string_t resp{"HTTP/1.1 200 OK\r\n"
                              "Server: Apache\r\n"
                              "ETag: 1450013-6514-e905eec0\r\n"
                              "Connection: close\r\n"
                              "Content-Length: 4\r\n"
                              "Content-Type: application/x-bittorrent\r\n\r\n"};
    constexpr const_string_t resp2{"aaaa"};
    constexpr auto resp_content_len = resp2.size();

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_REQUIRE(!trans.in_http_tunnel());
    // Now set the transaction forcefully in HTTP tunnel
    trans.force_http_tunnel();
    BOOST_REQUIRE(trans.in_http_tunnel());

    // Now the transaction should complete but without cache_key and
    // content-encoding
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp2.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    // However the content length must be present
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(),
                      resp.size() + resp2.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size() + resp2.size());
    BOOST_CHECK(!trans.get_cache_key());
}

BOOST_AUTO_TEST_CASE(get_digest_sha1_when_no_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Want-Digest: MD5;q=0.3, sha;q=1\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{
        "HTTP/1.1 200 OK\r\n"
        "Server: Microsoft-IIS/8.0\r\n"
        "Content-Encoding: gzip\r\n"
        "Digest: SHA=thvDyvhfIqlvFe+A9MYgxAfm1q5=,unixsum=30637\r\n"
        "Content-Length: 10\r\n\r\n"
        "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;
    const string_view_t url         = "www.httpwatch.com/httpgallery/compression/";
    const string_view_t encoding    = "gzip";
    const string_view_t digest_sha1 = "thvDyvhfIqlvFe+A9MYgxAfm1q5=";

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK_EQUAL(ckey->content_encoding_, encoding);
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK_EQUAL(ckey->digest_sha1_, digest_sha1);
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(get_digest_md5_when_no_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Want-Digest: MD5;q=0.3, sha;q=1\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{
        "HTTP/1.1 200 OK\r\n"
        "Server: Microsoft-IIS/8.0\r\n"
        "Content-Encoding: gzip\r\n"
        "Digest: unixsum=30637,md5=HUXZLQLMuI/KZ5KDcJPcOA==\r\n"
        "Content-Length: 10\r\n\r\n"
        "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;
    const string_view_t url         = "www.httpwatch.com/httpgallery/compression/";
    const string_view_t encoding    = "gzip";
    const string_view_t digest_md5  = "HUXZLQLMuI/KZ5KDcJPcOA==";

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK_EQUAL(ckey->content_encoding_, encoding);
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK_EQUAL(ckey->digest_md5_, digest_md5);
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);
}

BOOST_AUTO_TEST_CASE(get_no_digest_when_http_tunnel)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Want-Digest: MD5;q=0.3, sha;q=1\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"};
    constexpr const_string_t resp2{
        "Server: Microsoft-IIS/8.0\r\n"
        "Content-Encoding: gzip\r\n"
        "Digest: SHA=thvDyvhfIqlvFe+A9MYgxAfm1q5=,unixsum=30637\r\n"
        "Content-Length: 10\r\n\r\n"
        "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_REQUIRE(!trans.in_http_tunnel());
    // Now set the transaction forcefully in HTTP tunnel
    trans.force_http_tunnel();
    BOOST_REQUIRE(trans.in_http_tunnel());

    // Now the transaction should complete but without cache_key and
    // content-encoding
    ret = trans.on_resp_data((const uint8_t*)resp2.data(), resp2.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp2.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    // However the content length must be present
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(),
                      resp.size() + resp2.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size() + resp2.size());
    BOOST_CHECK(!trans.get_cache_key());
}

BOOST_AUTO_TEST_CASE(go_to_http_tunnel_on_multiple_digests)
{
    constexpr const_string_t req{"GET /httpgallery/compression/ HTTP/1.1\r\n"
                                 "Accept-Encoding:gzip, deflate, sdch, br\r\n"
                                 "Want-Digest: MD5;q=0.3, sha;q=1\r\n"
                                 "Host: www.httpwatch.com\r\n"
                                 "Connection: keep-alive\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Server: Microsoft-IIS/8.0\r\n"
                                  "Content-Encoding: gzip\r\n"
                                  "Digest: "
                                  "SHA=thvDyvhfIqlvFe+A9MYgxAfm1q5=,unixsum="
                                  "30637,md5=HUXZLQLMuI/KZ5KDcJPcOA==\r\n"
                                  "Content-Length: 10\r\n\r\n"
                                  "xxxxxxxxxx"};
    constexpr auto resp_content_len = 10;

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size() - resp_content_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must not have valid cache key
    BOOST_CHECK(!trans.get_cache_key());
}

BOOST_AUTO_TEST_CASE(get_content_range_when_no_http_tunnel)
{
    const const_string_t req{"GET /BigBuckBunny_320x180.mp4 HTTP/1.1\r\n"
                             "Connection: keep-alive\r\n"
                             "Host: localhost:8080\r\n"
                             "Range: bytes=64312833-64657026\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept-Encoding: identity\r\n\r\n"};
    const const_string_t resp{
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: video/mp4\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 344194\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes 64312833-64657026/64657027\r\n\r\n"};
    const string_view_t url = "localhost:8080/BigBuckBunny_320x180.mp4";
    const cache::cache_key::rng rng{64312833, 64657026};
    constexpr auto resp_content_len = 344194;

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    // The body is not yet received
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key
    const auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(ckey->rng_ == rng);
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);

    // Forcefully end it
    trans.on_resp_end_of_stream();
    BOOST_CHECK(trans.resp_completed());
}

BOOST_AUTO_TEST_CASE(get_no_content_range_when_http_tunnel)
{
    const const_string_t req{"GET /BigBuckBunny_320x180.mp4 HTTP/1.1\r\n"
                             "Connection: keep-alive\r\n"
                             "Host: localhost:8080\r\n"
                             "Range: bytes=64312833-64657026\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept-Encoding: identity\r\n\r\n"};
    // The missing 'Content-Length' will enter us in http-tunnel mode and
    // we'll get no cache key.
    // In addition we'll skip the body and complete without it,
    // because the content-length is missing.
    const const_string_t resp{
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: video/mp4\r\n"
        "Connection: keep-alive\r\n"
        //"Content-Length: 344194\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes 64312833-64657026/64657027\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    // The body is not yet received
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());

    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(trans.resp_completed());
    BOOST_CHECK(!trans.resp_content_len());
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp.size());
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp.size());
    // We must have valid cache key
    BOOST_CHECK(!trans.get_cache_key());
}

BOOST_AUTO_TEST_CASE(go_to_error_on_invalid_rng1)
{
    const const_string_t req{"GET /BigBuckBunny_320x180.mp4 HTTP/1.1\r\n"
                             "Connection: keep-alive\r\n"
                             "Host: localhost:8080\r\n"
                             "Range: bytes=64312833-64657026\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept-Encoding: identity\r\n\r\n"};
    // Range end is greater or equal to the full object length
    const const_string_t resp{
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: video/mp4\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 344194\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes 64312833-64657026/64657026\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    // The body is not yet received
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::error);
}

BOOST_AUTO_TEST_CASE(go_to_error_on_invalid_rng2)
{
    const const_string_t req{"GET /BigBuckBunny_320x180.mp4 HTTP/1.1\r\n"
                             "Connection: keep-alive\r\n"
                             "Host: localhost:8080\r\n"
                             "Range: bytes=64312833-64657026\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept-Encoding: identity\r\n\r\n"};
    // The begin and the end of the range are swapped
    const const_string_t resp{
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: video/mp4\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 344194\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes 64657026-64312833/64657027\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    // The body is not yet received
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::error);
}

// The range length is different than the content length.
// The content-length info is first.
BOOST_AUTO_TEST_CASE(go_to_error_on_rng_vs_cont_len_1)
{
    const const_string_t req{"GET /BigBuckBunny_320x180.mp4 HTTP/1.1\r\n"
                             "Connection: keep-alive\r\n"
                             "Host: localhost:8080\r\n"
                             "Range: bytes=64312833-64657026\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept-Encoding: identity\r\n\r\n"};
    const const_string_t resp{
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: video/mp4\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 344194\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes 64312833-64657025/64657027\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    // The body is not yet received
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::error);
}

// The range length is different than the content length.
// The content-length info is second.
BOOST_AUTO_TEST_CASE(go_to_error_on_rng_vs_cont_len_2)
{
    const const_string_t req{"GET /BigBuckBunny_320x180.mp4 HTTP/1.1\r\n"
                             "Connection: keep-alive\r\n"
                             "Host: localhost:8080\r\n"
                             "Range: bytes=64312833-64657026\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept-Encoding: identity\r\n\r\n"};
    const const_string_t resp{
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: video/mp4\r\n"
        "Connection: keep-alive\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes 64312833-64657025/64657027\r\n"
        "Content-Length: 344194\r\n\r\n"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    // The body is not yet received
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::error);
}

BOOST_AUTO_TEST_CASE(test_move_construction)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "User-Agent: HTTPTool/1.0\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: 18\r\n\r\n"};
    constexpr const_string_t body{"<html>shits</html>"};
    constexpr auto resp_content_len = body.size();
    constexpr auto resp_hdrs_len    = resp.size();
    const string_view_t url{"w3schools.com/test/demo_form.asp"};

    id_tag tag;
    http_trans trans(tag);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE(ret.res_ == http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    // Now the headers becomes completed and the cache key is present
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp_hdrs_len);
    // We must have valid cache key with URL and content-length only.
    auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);

    // Now after the move the old transaction should become empty
    http_trans trans2(std::move(trans));

    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
    BOOST_REQUIRE(!trans.get_cache_key());

    // And the trans2 now have the same properties as the trans and
    // can be finished by parsing the body.
    BOOST_CHECK(trans2.req_hdrs_completed());
    BOOST_CHECK(trans2.req_completed());
    BOOST_CHECK(!trans2.req_content_len());
    BOOST_CHECK(trans2.resp_hdrs_completed());
    BOOST_CHECK(!trans2.resp_completed());
    BOOST_REQUIRE(trans2.resp_content_len());
    BOOST_CHECK_EQUAL(*trans2.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans2.is_keep_alive());
    BOOST_CHECK(!trans2.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans2.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans2.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans2.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans2.resp_bytes(), resp_hdrs_len);
    // We must have valid cache key with URL and content-length only.
    ckey = trans2.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);

    // Now finish it
    ret = trans2.on_resp_data((const uint8_t*)body.data(), body.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, body.size());
    BOOST_CHECK(trans2.resp_completed());
    BOOST_CHECK_EQUAL(trans2.resp_bytes(), resp_hdrs_len + body.size());
}

BOOST_AUTO_TEST_CASE(test_move_assignment)
{
    constexpr const_string_t req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                                 "Host: w3schools.com\r\n"
                                 "User-Agent: HTTPTool/1.0\r\n\r\n"};
    constexpr const_string_t resp{"HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: 18\r\n\r\n"};
    constexpr const_string_t body{"<html>shits</html>"};
    constexpr auto resp_content_len = body.size();
    constexpr auto resp_hdrs_len    = resp.size();
    const string_view_t url{"w3schools.com/test/demo_form.asp"};

    id_tag tag, tag2;
    http_trans trans(tag);
    http_trans trans2(tag2);

    auto ret = trans.on_req_data((const uint8_t*)req.data(), req.size());
    BOOST_REQUIRE(ret.res_ == http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, req.size());

    // Now the headers becomes completed and the cache key is present
    ret = trans.on_resp_data((const uint8_t*)resp.data(), resp.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::proceed);
    BOOST_REQUIRE_EQUAL(ret.consumed_, resp.size());
    BOOST_CHECK(trans.req_hdrs_completed());
    BOOST_CHECK(trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_REQUIRE(trans.resp_content_len());
    BOOST_CHECK_EQUAL(*trans.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), resp_hdrs_len);
    // We must have valid cache key with URL and content-length only.
    auto ckey = trans.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);

    // Now after the move the old transaction should become empty
    trans2 = std::move(trans);

    BOOST_CHECK(!trans.req_hdrs_completed());
    BOOST_CHECK(!trans.req_completed());
    BOOST_CHECK(!trans.req_content_len());
    BOOST_CHECK(!trans.resp_hdrs_completed());
    BOOST_CHECK(!trans.resp_completed());
    BOOST_CHECK(!trans.resp_content_len());
    BOOST_CHECK(!trans.is_keep_alive());
    BOOST_CHECK(!trans.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans.req_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.req_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.resp_hdrs_bytes(), 0);
    BOOST_CHECK_EQUAL(trans.resp_bytes(), 0);
    BOOST_REQUIRE(!trans.get_cache_key());

    // And the trans2 now have the same properties as the trans and
    // can be finished by parsing the body.
    BOOST_CHECK(trans2.req_hdrs_completed());
    BOOST_CHECK(trans2.req_completed());
    BOOST_CHECK(!trans2.req_content_len());
    BOOST_CHECK(trans2.resp_hdrs_completed());
    BOOST_CHECK(!trans2.resp_completed());
    BOOST_REQUIRE(trans2.resp_content_len());
    BOOST_CHECK_EQUAL(*trans2.resp_content_len(), resp_content_len);
    // HTTP 1.1 request and response i.e. the keep-alive should be on
    BOOST_CHECK(trans2.is_keep_alive());
    BOOST_CHECK(!trans2.in_http_tunnel());
    BOOST_CHECK_EQUAL(trans2.req_hdrs_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans2.req_bytes(), req.size());
    BOOST_CHECK_EQUAL(trans2.resp_hdrs_bytes(), resp_hdrs_len);
    BOOST_CHECK_EQUAL(trans2.resp_bytes(), resp_hdrs_len);
    // We must have valid cache key with URL and content-length only.
    ckey = trans2.get_cache_key();
    BOOST_REQUIRE(ckey);
    BOOST_CHECK(!ckey->rng_.valid());
    BOOST_CHECK(ckey->content_encoding_.empty());
    BOOST_CHECK(ckey->content_md5_.empty());
    BOOST_CHECK(ckey->digest_sha1_.empty());
    BOOST_CHECK(ckey->digest_md5_.empty());
    BOOST_CHECK(ckey->etag_.empty());
    BOOST_CHECK_EQUAL(ckey->url_, url);
    BOOST_CHECK_EQUAL(ckey->obj_full_len_, resp_content_len);

    // Now finish it
    ret = trans2.on_resp_data((const uint8_t*)body.data(), body.size());
    BOOST_REQUIRE_EQUAL((int)ret.res_, (int)http_trans::res::complete);
    BOOST_REQUIRE_EQUAL(ret.consumed_, body.size());
    BOOST_CHECK(trans2.resp_completed());
    BOOST_CHECK_EQUAL(trans2.resp_bytes(), resp_hdrs_len + body.size());
}

BOOST_AUTO_TEST_SUITE_END()
