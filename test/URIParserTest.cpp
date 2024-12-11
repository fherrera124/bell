#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "catch2/matchers/catch_matchers_string.hpp"

#include <sys/socket.h>
#include "bell/io/URIParser.h"

using Catch::Matchers::Equals;

TEST_CASE("bell::io::URIParser tests", "[bell::io::URIParser]") {
  SECTION("parses valid URIs") {
    // Parse first URI
    auto result = bell::uri::parse("http://www.example.com");
    REQUIRE(result.has_value());
    REQUIRE(result->port.has_value() == false);
    REQUIRE(result->query.has_value() == false);

    CHECK_THAT(result->scheme.value(), Equals("http"));
    CHECK_THAT(result->host.value(), Equals("www.example.com"));
    CHECK_THAT(result->path.value(), Equals("/"));

    // Parse second URI
    result = bell::uri::parse(
        "https://www.example.com:443/path/to/resource?query=value");
    REQUIRE(result.has_value());
    REQUIRE(result->port.value() == 443);

    CHECK_THAT(result->scheme.value(), Equals("https"));
    CHECK_THAT(result->host.value(), Equals("www.example.com"));
    CHECK_THAT(result->path.value(), Equals("/path/to/resource"));
    CHECK_THAT(result->query.value(), Equals("query=value"));

    // Parse third URI
    result = bell::uri::parse("ftp://ftp.example.com:21/resources");
    REQUIRE(result.has_value());
    REQUIRE(result->port.value() == 21);
    REQUIRE(result->query.has_value() == false);

    CHECK_THAT(result->scheme.value(), Equals("ftp"));
    CHECK_THAT(result->host.value(), Equals("ftp.example.com"));
    CHECK_THAT(result->path.value(), Equals("/resources"));
  }

  SECTION("fails to parse invalid URIs") {
    auto result = bell::uri::parse("://www.example.com");  // Missing scheme
    REQUIRE(result.has_value() == false);

    result = bell::uri::parse("http://:80");  // Missing host
    REQUIRE(result.has_value() == false);

    result = bell::uri::parse("http://www.example.com:abc");  // Invalid port
    REQUIRE(result.has_value() == false);

    result = bell::uri::parse("http://www.example.com?");  // Empty query
    REQUIRE(result.has_value() == false);
  }
}