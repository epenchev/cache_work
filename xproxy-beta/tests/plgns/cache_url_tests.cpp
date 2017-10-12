#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../plgns/cache_url.h"

BOOST_AUTO_TEST_SUITE(cache_url_tests)

BOOST_AUTO_TEST_CASE(google_urls_match)
{
    // clang-format off
    const std::string cfg_data =
    {
        "^http://[^\\.]+\\.(ytimg\\.com)(.*)   http://$1.x3me/$2\n"
        "^http://[^\\.]+\\.(ggpht\\.com)(.*)   http://$1.x3me/$2\n"
        "^http://t\\d+\\.(gstatic\\.com)/(.*)  http://$1.x3me/$2\n"
        "^http://pagead\\d+\\.(googlesyndication\\.com)/(.*)   http://$1.x3me/$2\n"
        "^http://(fonts\\.googleapis\\.com)/css\\?(.*)     http://$1.x3me/$2\n"
        "^http://[^/]*\\.c\\.android\\.clients\\.google\\.com/([^\\?]+) http://c.android.x3me/$1\n"
        "^http://(mw[0-9]+|kh)\\.google\\.com/(.*)    http://gearth.x3me/$2\n"
        "^http://\\d+\\.(bp\\.blogspot\\.com)/(.*)  http://$1.x3me/$2\n"
    };
    // clang-format on
    std::istringstream cfg(cfg_data);

    plgns::cache_url cache_url;

    try
    {
        cache_url.init(cfg);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }

    std::string new_url;
    cache_url.produce_cache_url("http://abc.ytimg.com/hey", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://ytimg.com.x3me//hey");
    cache_url.produce_cache_url("http://t234.gstatic.com/hey/mey", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://gstatic.com.x3me/hey/mey");
    cache_url.produce_cache_url(
        "http://aaa123.c.android.clients.google.com/hey?mey=1", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://c.android.x3me/hey");

    cache_url.produce_cache_url("http://mw90.google.com/i_am_here", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://gearth.x3me/i_am_here");
}

BOOST_AUTO_TEST_CASE(google_urls_no_match)
{
    // clang-format off
    const std::string cfg_data =
    {
        "^http://[^\\.]+\\.(ytimg\\.com)(.*)   http://$1.x3me/$2\n"
        "^http://[^\\.]+\\.(ggpht\\.com)(.*)   http://$1.x3me/$2\n"
        "^http://t\\d+\\.(gstatic\\.com)/(.*)  http://$1.x3me/$2\n"
        "^http://pagead\\d+\\.(googlesyndication\\.com)/(.*)   http://$1.x3me/$2\n"
        "^http://(fonts\\.googleapis\\.com)/css\\?(.*)     http://$1.x3me/$2\n"
        "^http://[^/]*\\.c\\.android\\.clients\\.google\\.com/([^\\?]+) http://c.android.x3me/$1\n"
        "^http://(mw[0-9]+|kh)\\.google\\.com/(.*)    http://gearth.x3me/$2\n"
        "^http://\\d+\\.(bp\\.blogspot\\.com)/(.*)  http://$1.x3me/$2\n"
    };
    // clang-format on
    std::istringstream cfg(cfg_data);

    plgns::cache_url cache_url;

    try
    {
        cache_url.init(cfg);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }

    std::string new_url;
    cache_url.produce_cache_url("http://abc.ytimg1.com/hey", new_url);
    BOOST_REQUIRE(new_url.empty());
    cache_url.produce_cache_url("http://t234a.gstatic.com/hey/mey", new_url);
    BOOST_REQUIRE(new_url.empty());
    cache_url.produce_cache_url(
        "http://aaa123.d.android.clients.google.com/hey?mey=1", new_url);
    BOOST_REQUIRE(new_url.empty());

    cache_url.produce_cache_url("http://bmw90.google.com/i_am_here", new_url);
    BOOST_REQUIRE(new_url.empty());
}

BOOST_AUTO_TEST_CASE(apple_urls_match)
{
    // clang-format off
    const std::string cfg_data =
    {
        "^http://[^/]+?\\.((itunes\\.apple|mzstatic|apple|cdn-apple)\\.com)"
            "/([^\\?]+\\.(jpe?g|png|gif|tiff|ipsw|(pf)"
            "?pkg|ipa|dmg|m4[p|v]|zip))(\\?.*)?$     http://$1.x3me/$3"
    };
    // clang-format on
    std::istringstream cfg(cfg_data);

    plgns::cache_url cache_url;

    try
    {
        cache_url.init(cfg);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }

    std::string new_url;
    cache_url.produce_cache_url("http://abc.itunes.apple.com/hey.jpg", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://itunes.apple.com.x3me/hey.jpg");
    cache_url.produce_cache_url("http://abc.itunes.mzstatic.com/so.pkg?if=345",
                                new_url);
    BOOST_CHECK_EQUAL(new_url, "http://mzstatic.com.x3me/so.pkg");
    cache_url.produce_cache_url(
        "http://abc.itunes.cdn-apple.com/so.m4p?of=aaaa", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://cdn-apple.com.x3me/so.m4p");
}

BOOST_AUTO_TEST_CASE(XXX_urls_match)
{
    // clang-format off
    const std::string cfg_data =
    {
        "^(http://.+[&\\?](fs|ms|start)=\\d+.*)$  $1\n"
        "^http://[^/]+?\\.xvideos\\.com/videos/[^\\?]+/xvideos\\.com_"
            "([a-z0-9]{3}\\.(flv|mp4))(\\?.*)?$  http://xvideos.com.x3me/$1\n"
        "^http://img[\\d\\-][^/]+?\\.xvideos\\.com/(.*)	http://img.xvideos.com.x3me/$1\n"
        "^http://[^/]+?\\.((cdn13|contentabc)\\.com)/"
            "([^\\?]+(flv|mp4))(\\?.*)?$      http://$1.phak/$3\n"
        "^http://[^/]+?\\.(rncdn3|redtubefiles|phncdn)\\.com/"
            "[^\\?]+/([^/]+\\.(flv|mp4))(\\?.*)?$	http://phncdn.phak/$2\n"
        "^http://((\\d+\\.\\d+\\.\\d+\\.\\d+)|(xhcdn\\.com))/"
            "key=.+?/(\\d+\\.(flv|mp4))(\\?.*)?$   http://xhcdn.com.phak/$4\n"
    };
    // clang-format on
    std::istringstream cfg(cfg_data);

    plgns::cache_url cache_url;

    try
    {
        cache_url.init(cfg);
    }
    catch (const std::exception& ex)
    {
        BOOST_REQUIRE_MESSAGE(false, ex.what());
    }

    std::string new_url;
    cache_url.produce_cache_url("http://alabala.com?hey=1&fs=123", new_url);
    BOOST_CHECK_EQUAL(new_url, "http://alabala.com?hey=1&fs=123");
    cache_url.produce_cache_url(
        "http://very-interesting.xvideos.com/videos/aa/xvideos.com_aaa.flv",
        new_url);
    BOOST_CHECK_EQUAL(new_url, "http://xvideos.com.x3me/aaa.flv");

    cache_url.produce_cache_url("http://123.123.123.123/key=abc/1234.flv",
                                new_url);
    BOOST_CHECK_EQUAL(new_url, "http://xhcdn.com.phak/1234.flv");
}

BOOST_AUTO_TEST_SUITE_END()
