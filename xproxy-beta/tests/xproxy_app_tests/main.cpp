#include "precompiled.h"
#define BOOST_TEST_MODULE xproxy_test
#include <boost/test/unit_test.hpp>

#include "tcp_client.h"
#include "tcp_server.h"
#include "utils.h"

using namespace std::literals::chrono_literals;
using boost::string_view;

static const auto random_data = gen_random_data(16_KB);
static const auto rdata_16KB  = string_view{random_data.data(), 16_KB};
static const auto rdata_8KB   = string_view{random_data.data(), 8_KB};
static const auto rdata_4KB   = string_view{random_data.data(), 4_KB};
static const auto rdata_2KB   = string_view{random_data.data(), 2_KB};
static const auto rdata_1KB   = string_view{random_data.data(), 1_KB};

////////////////////////////////////////////////////////////////////////////////

struct fixture
{
    tcp_client client;
    tcp_server server;

    void start()
    {
        static const boost::string_view proxy_ip  = "192.168.1.9";
        static const boost::string_view server_ip = "192.168.168.123";
        static const uint16_t proxy_port          = 12345;
        static const uint16_t server_port         = 54321;

        auto ret1 =
            std::async(std::launch::async, [&]
                       {
                           // std::cout << "Server start listening on "
                           //          << server_ip << ':' << server_port <<
                           //          '\n';
                           return server.accept_on(server_ip, server_port);
                       });
        auto ret2 = std::async(std::launch::async, [&]
                               {
                                   // std::cout << "Client start connecting to "
                                   // << proxy_ip << ':'
                                   //          << proxy_port << '\n';
                                   return client.connect_to(server_ip, proxy_ip,
                                                            proxy_port);
                               });
        BOOST_REQUIRE_MESSAGE(ret1.get(), server.last_err());
        BOOST_REQUIRE_MESSAGE(ret2.get(), client.last_err());
    }
};

// Sleep is needed so that we can ensure that the waited data is received.
// However, this is not correct approach, but it's good enough for here (IMO).
// Thus the function is called stupid_sleep.
void stupid_sleep(std::chrono::milliseconds ms)
{
    std::this_thread::sleep_for(ms);
}

template <typename Rdr>
void read_chunk(Rdr& rdr, const string_view& chunk)
{
    string_view read_chunk;
    for (uint32_t rlen = 0; rlen < chunk.size(); rlen += read_chunk.size())
    {
        read_chunk = rdr.read_some(16_KB);
        BOOST_REQUIRE_MESSAGE(!read_chunk.empty(), rdr.last_err());
        BOOST_REQUIRE(chunk.size() - rlen >= read_chunk.size());
        BOOST_REQUIRE(read_chunk == chunk.substr(rlen, read_chunk.size()));
    }
};

////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(xproxy_test, fixture)

BOOST_AUTO_TEST_CASE(send_non_http_recv_non_http)
{
    start();

    // The client send data in parts
    BOOST_REQUIRE_MESSAGE(client.write(rdata_1KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_2KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_4KB), client.last_err());
    // The server send data in parts
    BOOST_REQUIRE_MESSAGE(server.write(rdata_4KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(rdata_2KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(rdata_1KB), client.last_err());

    stupid_sleep(200ms);

    const auto read_req = server.read_some(8_KB); // Try more
    BOOST_REQUIRE_MESSAGE(read_req.size() == 7_KB, server.last_err());
    BOOST_REQUIRE((read_req.substr(0, 1_KB) == rdata_1KB));
    BOOST_REQUIRE((read_req.substr(1_KB, 2_KB) == rdata_2KB));
    BOOST_REQUIRE((read_req.substr(3_KB, 4_KB) == rdata_4KB));

    const auto read_resp = client.read_some(8_KB); // Try more
    BOOST_REQUIRE_MESSAGE(read_resp.size() == 7_KB, client.last_err());
    BOOST_REQUIRE((read_resp.substr(0, 4_KB) == rdata_4KB));
    BOOST_REQUIRE((read_resp.substr(4_KB, 2_KB) == rdata_2KB));
    BOOST_REQUIRE((read_resp.substr(6_KB, 1_KB) == rdata_1KB));
}

// This enters the proxy in blind tunnel mode
BOOST_AUTO_TEST_CASE(server_talks_first_and_closes_send_dir)
{
    const string_view req_part0{"GET /test/demo_file HTTP/1.1\r\n"};
    const string_view req_part1{"Host: w3schools.com\r\n"
                                "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/binary\r\n"
                           "Content-Length: 6144\r\n\r\n"};

    start();

    // Let's say that the server is stupidly written, and starts to
    // respond first after a connect, before a request is received.
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req_part0), client.last_err());
    // Now the server sends the actual body data and closes the send direction.
    BOOST_REQUIRE_MESSAGE(server.write(rdata_4KB), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(rdata_2KB), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());
    // Lastly the client sends the remaining of the request
    BOOST_REQUIRE_MESSAGE(client.write(req_part1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), server.last_err());

    stupid_sleep(500ms);

    // All data must have been received by the client and by the server.
    const auto read_req =
        server.read_some(req_part0.size() + req_part1.size() + 42);
    BOOST_REQUIRE_MESSAGE(
        (read_req.size() == (req_part0.size() + req_part1.size())),
        server.last_err());
    BOOST_REQUIRE_EQUAL(read_req.substr(0, req_part0.size()), req_part0);
    BOOST_REQUIRE_EQUAL(read_req.substr(req_part0.size()), req_part1);
    const auto read_resp = client.read_some(resp.size());
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp, resp);
    const auto read_body = client.read_some(7_KB);
    BOOST_REQUIRE_MESSAGE(read_body.size() == 6_KB, client.last_err());
    BOOST_REQUIRE((read_body.substr(0, 4_KB) == rdata_4KB));
    BOOST_REQUIRE((read_body.substr(4_KB, 2_KB) == rdata_2KB));

    // Now if we close the server connection, the client must receive EOF
    // because it has read all the data.
    server.close();
    const auto read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

// This enters the proxy in blind tunnel mode
BOOST_AUTO_TEST_CASE(server_talks_early_and_closes_send_dir)
{
    const string_view req_part0{"GET /test/demo_file HTTP/1.1\r\n"};
    const string_view req_part1{"Host: w3schools.com\r\n"
                                "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/binary\r\n"
                           "Content-Length: 6144\r\n\r\n"};

    start();

    // Let's say that the server is stupidly written, and starts to
    // respond sending the response after receiving the first request line.
    BOOST_REQUIRE_MESSAGE(client.write(req_part0), client.last_err());
    const auto read_req0 = server.read_some(req_part0.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req0.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req0, req_part0);
    // The server starts sending the response
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    // Now the server sends the actual body data and closes the send direction.
    BOOST_REQUIRE_MESSAGE(server.write(rdata_4KB), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(rdata_2KB), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());
    // Lastly the client sends the remaining of the request
    BOOST_REQUIRE_MESSAGE(client.write(req_part1), client.last_err());

    // All data must have been received by the client and by the server.
    const auto read_req1 = server.read_some(req_part1.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req1.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req1, req_part1);
    const auto read_resp = client.read_some(resp.size());
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp, resp);
    stupid_sleep(400ms);
    const auto read_body = client.read_some(7_KB);
    BOOST_REQUIRE_MESSAGE(read_body.size() == 6_KB, client.last_err());
    BOOST_REQUIRE((read_body.substr(0, 4_KB) == rdata_4KB));
    BOOST_REQUIRE((read_body.substr(4_KB, 2_KB) == rdata_2KB));

    // The client must receive EOF because it has read all the data.
    const auto read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());

    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), server.last_err());
}

BOOST_AUTO_TEST_CASE(req_get_no_hdrs_resp_ok_no_hdrs)
{
    const string_view req{"GET /some_path HTTP/1.1\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n\r\n"};

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req), client.last_err());
    const auto read_req = server.read_some(req.size());
    BOOST_REQUIRE_MESSAGE(!read_req.empty(), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    const auto read_resp = client.read_some(resp.size());
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());

    BOOST_REQUIRE_EQUAL(req, read_req);
    BOOST_REQUIRE_EQUAL(resp, read_resp);
}

BOOST_AUTO_TEST_CASE(req_get_with_hdrs_resp_ok_with_hdrs_body)
{
    const string_view req_part0{"GET /test/demo_form.asp HTTP/1.1\r\n"};
    const string_view req_part1{"Host: w3schools.com\r\n"
                                "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: 18\r\n\r\n"};
    const string_view body{"<html>shits</html>"};

    start();

    // The client sends the request in two parts first.
    BOOST_REQUIRE_MESSAGE(client.write(req_part0), client.last_err());
    // Simply ask to receive something more. We shouldn't receive it though.
    auto read_req = server.read_some(req_part0.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req, req_part0);
    BOOST_REQUIRE_MESSAGE(client.write(req_part1), client.last_err());
    read_req = server.read_some(req_part1.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req, req_part1);

    // The server sends the response in two parts headers and body
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    auto read_resp = client.read_some(resp.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(resp, read_resp);
    BOOST_REQUIRE_MESSAGE(server.write(body), server.last_err());
    read_resp = client.read_some(body.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(body, read_resp);
}

// The server responses and shutdowns the send direction after that.
BOOST_AUTO_TEST_CASE(req_post_with_hdrs_and_body_resp_ok_shutdown_both)
{
    const string_view req{"POST /test/best_bank HTTP/1.1\r\n"
                          "Host: w3schools.com\r\n"
                          "Content-Type: application/binary\r\n"
                          "Content-Length: 8192\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: 15\r\n\r\n"
                           "<html>OK</html>"};

    start();

    // The client sends a request and body and shutdowns the send direction.
    BOOST_REQUIRE_MESSAGE(client.write(req), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_4KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_2KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_2KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());

    // The server sends the response
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());

    auto read_resp = client.read_some(resp.size());
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp, resp);
    // Now if the client read again it should get EOF, because the both
    // direction has been shut down and the connection should have been closed.
    auto read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());

    // Now the server should receive the full request and EOF at the end.
    auto read_req = server.read_some(req.size());
    BOOST_REQUIRE_MESSAGE(!read_req.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_req, req);
    auto read_body = server.read_some(9_KB);
    BOOST_REQUIRE_MESSAGE(read_body.size() == 8_KB, server.last_err());
    BOOST_REQUIRE((read_body.substr(0, 4_KB) == rdata_4KB));
    BOOST_REQUIRE((read_body.substr(4_KB, 2_KB) == rdata_2KB));
    BOOST_REQUIRE((read_body.substr(6_KB, 2_KB) == rdata_2KB));
    // Now we should get EOF
    read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());
}

// We know that we break the pipeline-ing.
// All requests are cacheable.
BOOST_AUTO_TEST_CASE(pipelined_requests_are_received_one_by_one)
{
    const string_view req0{"GET /test/demo_form0.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req1{"GET /test/demo_form1.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req2{"GET /test/demo_file2 HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};

    const string_view resp0{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit0</html>"};
    const string_view resp1{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit1</html>"};
    const string_view resp2{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit2</html>"};

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req0), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req2), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());

    // Now the server shouldn't receive request (after the first one),
    // before it sends a response.
    auto read_req0 = server.read_some(req0.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req0.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req0, req0);
    BOOST_REQUIRE_MESSAGE(server.write(resp0), server.last_err());
    auto read_req1 = server.read_some(req1.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req1.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req1, req1);
    BOOST_REQUIRE_MESSAGE(server.write(resp1), server.last_err());
    auto read_req2 = server.read_some(req2.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req2.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req2, req2);
    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());

    BOOST_REQUIRE_MESSAGE(server.write(resp2), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());
    // The client must have received all of the responses.
    auto read_resp0 = client.read_some(resp0.size());
    BOOST_REQUIRE_MESSAGE(!read_resp0.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp0, resp0);
    auto read_resp1 = client.read_some(resp1.size());
    BOOST_REQUIRE_MESSAGE(!read_resp1.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp1, resp1);
    auto read_resp2 = client.read_some(resp2.size());
    BOOST_REQUIRE_MESSAGE(!read_resp2.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp2, resp2);

    // Now the client should get EOF if it tries to read again.
    read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

BOOST_AUTO_TEST_CASE(pipelined_requests_with_http_tunnel_in_between)
{
    const string_view req0{"GET /test/demo_form0.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req1{"POST /test/best_bank HTTP/1.1\r\n"
                           "Host: w3schools.com\r\n"
                           "Content-Type: application/binary\r\n"
                           "Content-Length: 3072\r\n\r\n"};
    const string_view req2{"GET /test/demo_file1 HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};

    const string_view resp0{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit0</html>"};
    const string_view resp1{"HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 15\r\n\r\n"
                            "<html>OK</html>"};
    const string_view resp2{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit2</html>"};

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req0), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_2KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(rdata_1KB), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req2), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());

    // Now the server shouldn't receive request (after the first one),
    // before it sends a response.
    auto read_req0 = server.read_some(req0.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req0.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req0, req0);
    BOOST_REQUIRE_MESSAGE(server.write(resp0), server.last_err());
    auto read_req1 = server.read_some(req1.size());
    BOOST_REQUIRE_MESSAGE(!read_req1.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req1, req1);
    stupid_sleep(500ms);
    auto read_body1 = server.read_some(4_KB); // Should read 3_KB
    BOOST_REQUIRE_MESSAGE(read_body1.size() == 3_KB, server.last_err());
    BOOST_REQUIRE_EQUAL(read_body1.substr(0, 2_KB), rdata_2KB);
    BOOST_REQUIRE_EQUAL(read_body1.substr(2_KB, 1_KB), rdata_1KB);
    BOOST_REQUIRE_MESSAGE(server.write(resp1), server.last_err());
    auto read_req2 = server.read_some(req2.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req2.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req2, req2);
    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());

    BOOST_REQUIRE_MESSAGE(server.write(resp2), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());

    // The client must have received all of the responses.
    auto read_resp0 = client.read_some(resp0.size());
    BOOST_REQUIRE_MESSAGE(!read_resp0.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp0, resp0);
    auto read_resp1 = client.read_some(resp1.size());
    BOOST_REQUIRE_MESSAGE(!read_resp1.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp1, resp1);
    auto read_resp2 = client.read_some(resp2.size());
    BOOST_REQUIRE_MESSAGE(!read_resp2.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp2, resp2);

    // Now the client should get EOF if it tries to read again.
    read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

BOOST_AUTO_TEST_CASE(pipelined_requests_with_http_tunnel_between_responses)
{
    const string_view req0{"GET /test/demo_form0.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req1{"GET /test/demo_form1.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req2{"GET /test/demo_file1 HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};

    const string_view resp0{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit0</html>"};
    // The 'Transfer-Encoding' must put the proxy in HTTP tunnel mode.
    const string_view resp1{"HTTP/1.1 200 OK\r\n"
                            "Date: Fri, 31 Dec 1999 23:59:59 GMT\r\n"
                            "Content-Type: text/plain\r\n"
                            "Transfer-Encoding: chunked\r\n\r\n"};
    const string_view resp1_body{"1a; ignore-stuff-here\r\n"
                                 "abcdefghijklmnopqrstuvwxyz\r\n"
                                 "10\r\n"
                                 "1234567890abcdef\r\n"
                                 "0\r\n"
                                 "some-footer: some-value\r\n"
                                 "another-footer: another-value\r\n\r\n"};
    const string_view resp2{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit2</html>"};

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req0), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req2), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());

    // Now the server shouldn't receive request (after the first one),
    // before it sends a response.
    auto read_req0 = server.read_some(req0.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req0.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req0, req0);
    BOOST_REQUIRE_MESSAGE(server.write(resp0), server.last_err());
    auto read_req1 = server.read_some(req1.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req1.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req1, req1);
    BOOST_REQUIRE_MESSAGE(server.write(resp1), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp1_body), server.last_err());
    auto read_req2 = server.read_some(req2.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req2.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req2, req2);
    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());

    BOOST_REQUIRE_MESSAGE(server.write(resp2), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());

    // The client must have received all of the responses.
    auto read_resp0 = client.read_some(resp0.size());
    BOOST_REQUIRE_MESSAGE(!read_resp0.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp0, resp0);
    auto read_resp1 = client.read_some(resp1.size());
    BOOST_REQUIRE_MESSAGE(!read_resp1.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp1, resp1);
    auto read_resp1_body = client.read_some(resp1_body.size());
    BOOST_REQUIRE_MESSAGE(!read_resp1_body.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp1_body, resp1_body);
    auto read_resp2 = client.read_some(resp2.size());
    BOOST_REQUIRE_MESSAGE(!read_resp2.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp2, resp2);

    // Now the client should get EOF if it tries to read again.
    read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

BOOST_AUTO_TEST_CASE(pipelined_requests_blind_tunnel_from_second)
{
    const string_view req0{"GET /test/demo_form0.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    // A GET request without Content-Length, but with content makes to proxy
    // to go to blind-tunnel mode and delivers the pipelined requests
    // all at once (as they are sent).
    const string_view req1{"GET /test/demo_form1.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req1_body = rdata_2KB;
    const string_view req2{"GET /test/demo_file1 HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view resp0{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit0</html>"};
    const string_view resp1{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit1</html>"};
    const string_view resp2{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit2</html>"};

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req0), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1_body), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req2), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());

    // Now the server shouldn't receive request (after the first one),
    // before it sends a response.
    // The server must receive all of the pipelined requests at once because
    // the proxy has gone in blind-tunnel mode
    stupid_sleep(500ms);
    const auto exp_len =
        req0.size() + req1.size() + req1_body.size() + req2.size();
    auto read_data = server.read_some(exp_len);
    BOOST_REQUIRE_MESSAGE(read_data.size() == exp_len, server.last_err());
    BOOST_REQUIRE_EQUAL(read_data.substr(0, req0.size()), req0);
    BOOST_REQUIRE_EQUAL(read_data.substr(req0.size(), req1.size()), req1);
    BOOST_REQUIRE((read_data.substr(req0.size() + req1.size(),
                                    req1_body.size()) == req1_body));
    BOOST_REQUIRE_EQUAL(
        read_data.substr(req0.size() + req1.size() + req1_body.size(),
                         req2.size()),
        req2);
    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());

    BOOST_REQUIRE_MESSAGE(server.write(resp0), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp1), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp2), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());

    // The client must have received all of the responses.
    auto read_resp0 = client.read_some(resp0.size());
    BOOST_REQUIRE_MESSAGE(!read_resp0.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp0, resp0);
    auto read_resp1 = client.read_some(resp1.size());
    BOOST_REQUIRE_MESSAGE(!read_resp1.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp1, resp1);
    auto read_resp2 = client.read_some(resp2.size());
    BOOST_REQUIRE_MESSAGE(!read_resp2.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp2, resp2);

    // Now the client should get EOF if it tries to read again.
    read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

BOOST_AUTO_TEST_CASE(pipelined_requests_and_blind_tunnel_from_first_response)
{
    const string_view req0{"GET /test/demo_form0.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req1{"GET /test/demo_form1.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req2{"GET /test/demo_file1 HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};

    // Sending a response without 'Content-Length' but with body,
    // must provoke the xproxy to go to blind tunnel mode.
    const string_view resp0{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n\r\n"
                            "<html>shit0</html>"};
    const string_view resp1{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit1</html>"};
    const string_view resp2{"HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 18\r\n\r\n"
                            "<html>shit2</html>"};

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req0), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req2), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());

    // Now the server shouldn't receive request (after the first one),
    // before it sends a response.
    auto read_req0 = server.read_some(req0.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req0.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req0, req0);
    BOOST_REQUIRE_MESSAGE(server.write(resp0), server.last_err());
    // Now the server should be able to read all the second and the third
    // requests at once, because of the above 'invalid' http response.
    // Maybe we need a sleep here, just so the xproxy has time for reaction???
    auto read_data = server.read_some(req1.size() + req2.size() + 42);
    BOOST_REQUIRE_EQUAL(read_data.size(), (req1.size() + req2.size()));
    BOOST_REQUIRE_EQUAL(read_data.substr(0, req1.size()), req1);
    BOOST_REQUIRE_EQUAL(read_data.substr(req1.size(), req2.size()), req2);
    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());

    BOOST_REQUIRE_MESSAGE(server.write(resp1), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp2), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());

    // The client must have received all of the responses.
    auto read_resp0 = client.read_some(resp0.size());
    BOOST_REQUIRE_MESSAGE(!read_resp0.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp0, resp0);
    auto read_resp1 = client.read_some(resp1.size());
    BOOST_REQUIRE_MESSAGE(!read_resp1.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp1, resp1);
    auto read_resp2 = client.read_some(resp2.size());
    BOOST_REQUIRE_MESSAGE(!read_resp2.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp2, resp2);

    // Now the client should get EOF if it tries to read again.
    read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

// All sent data must be received on the server
BOOST_AUTO_TEST_CASE(pipelined_requests_client_premature_close)
{
    const string_view req0{"GET /test/demo_form0.asp HTTP/1.0\r\n"
                           "Host: w3schools.com\r\n"
                           "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req1{"POST /test/best_bank HTTP/1.1\r\n"
                           "Host: w3schools.com\r\n"
                           "Content-Type: application/binary\r\n"
                           "Content-Length: 3072\r\n\r\n"};
    const string_view req1_body = rdata_2KB;

    start();

    BOOST_REQUIRE_MESSAGE(client.write(req0), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.write(req1_body), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.close(), client.last_err());

    const auto exp_len = req0.size() + req1.size() + req1_body.size();
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());
    stupid_sleep(500ms);
    // The server should be able to read all of the send data
    auto read_data = server.read_some(8_KB); // Just try to read more
    BOOST_REQUIRE_MESSAGE(read_data.size() == exp_len, server.last_err());
    BOOST_REQUIRE_EQUAL(read_data.substr(0, req0.size()), req0);
    BOOST_REQUIRE_EQUAL(read_data.substr(req0.size(), req1.size()), req1);
    BOOST_REQUIRE(read_data.substr(exp_len - req1_body.size()) == req1_body);

    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());
}

// All sent data must be received from the client
BOOST_AUTO_TEST_CASE(req_get_resp_ok_premature_close)
{
    const string_view req{"GET /test/demo_formAAA.asp HTTP/1.1\r\n"
                          "Host: w3schools.com\r\n"
                          "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: 16384000\r\n\r\n"};
    const string_view body = rdata_16KB;

    start();

    // The client sends the request in two parts first.
    BOOST_REQUIRE_MESSAGE(client.write(req), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(body), server.last_err());
    // "Ensure" that all is written before close
    stupid_sleep(500ms);
    // This should provoke the xproxy to receive error from the origin receiving
    // side, because the server has unread data in the socket buffers and thus
    // the premature close sends RST packet.
    BOOST_REQUIRE_MESSAGE(server.close(), server.last_err());

    auto read_resp = client.read_some(resp.size());
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(resp, read_resp);

    for (uint32_t rlen = 0; rlen < body.size(); rlen += read_resp.size())
    {
        read_resp = client.read_some(16_KB);
        BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
        BOOST_REQUIRE(body.size() - rlen >= read_resp.size());
        BOOST_REQUIRE(read_resp == body.substr(rlen, read_resp.size()));
    }

    // Now the client should get EOF if it tries to read again.
    auto read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

BOOST_AUTO_TEST_CASE(req_get_no_content_length_with_body)
{
    const string_view req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                          "Host: w3schools.com\r\n"
                          "User-Agent: HTTPTool/1.0\r\n\r\n"};
    const string_view req_body = rdata_2KB;
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: 18\r\n\r\n"
                           "<html>shits</html>"};

    start();

    // The client sends the request in two parts first.
    BOOST_REQUIRE_MESSAGE(client.write(req), client.last_err());
    auto read_req = server.read_some(req.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(req, read_req);
    BOOST_REQUIRE_MESSAGE(server.write(resp), server.last_err());
    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());

    BOOST_REQUIRE_MESSAGE(client.write(req_body), client.last_err());
    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());
    read_chunk(server, req_body);
    // Now the server should get EOF if it tries to read again.
    auto read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());

    stupid_sleep(200ms);
    auto read_resp = client.read_some(resp.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(resp, read_resp);

    // Now the client should get EOF if it tries to read again.
    read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());
}

BOOST_AUTO_TEST_CASE(big_req_and_resp)
{
    const string_view req{"GET /test/demo_form.asp HTTP/1.1\r\n"
                          "Host: w3schools.com\r\n"
                          "User-Agent: HTTPTool/1.0\r\n"
                          "Content-Length: 524288\r\n\r\n"};
    const string_view resp{"HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/binary\r\n"
                           "Content-Length: 524288\r\n\r\n"};

    start();

    // The client sends the request in two parts first.
    BOOST_REQUIRE_MESSAGE(client.write(req), client.last_err());
    BOOST_REQUIRE_MESSAGE(server.write(resp), client.last_err());
    auto read_req = server.read_some(req.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_req.empty(), server.last_err());
    BOOST_REQUIRE_EQUAL(read_req, req);
    auto read_resp = client.read_some(resp.size() + 42);
    BOOST_REQUIRE_MESSAGE(!read_resp.empty(), client.last_err());
    BOOST_REQUIRE_EQUAL(read_resp, resp);

    for (size_t i = 0; i < (512_KB / 8_KB); ++i)
    {
        BOOST_REQUIRE_MESSAGE(client.write(rdata_8KB), client.last_err());
        BOOST_REQUIRE_MESSAGE(server.write(rdata_8KB), server.last_err());
        read_chunk(client, rdata_8KB);
        read_chunk(server, rdata_8KB);
    }

    BOOST_REQUIRE_MESSAGE(server.shutdown_send(), server.last_err());
    // Now the client should get EOF if it tries to read again.
    auto read_empty = client.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(client.last_err().empty());

    BOOST_REQUIRE_MESSAGE(client.shutdown_send(), client.last_err());
    // Now the server should get EOF if it tries to read again.
    read_empty = server.read_some(1);
    BOOST_REQUIRE(read_empty.empty());
    BOOST_REQUIRE(server.last_err().empty());
}

BOOST_AUTO_TEST_SUITE_END()
