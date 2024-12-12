#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "catch2/matchers/catch_matchers_string.hpp"

#include <sys/socket.h>
#include "bell/io/URIParser.h"

using Catch::Matchers::Equals;

TEST_CASE("bell::io::URIParser tests", "[bell::io::URIParser]") {
  SECTION("Valid URIs") {
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

  SECTION("Invalid URIs") {
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

TEST_CASE("bell::uri::encode tests", "[bell::uri::encode]") {
  SECTION("Unreserved Characters") {
    std::string input =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    std::string expected =
        input;  // Unreserved characters should not be encoded
    REQUIRE(bell::uri::encode(input) == expected);
  }

  SECTION("Reserved Characters") {
    std::string input = "!*'();:@&=+$,/?#[]";
    std::string expected =
        "%21%2A%27%28%29%3B%3A%40%26%3D%2B%24%2C%2F%3F%23%5B%5D";
    REQUIRE(bell::uri::encode(input) == expected);
  }

  SECTION("Mixed Characters") {
    std::string input = "Hello, World!";
    std::string expected = "Hello%2C%20World%21";
    REQUIRE(bell::uri::encode(input) == expected);
  }

  SECTION("Non-ASCII Characters") {
    std::string input = "こんにちは";  // "Hello" in Japanese
    // Percent-encode the UTF-8 bytes
    std::string expected = "%E3%81%93%E3%82%93%E3%81%AB%E3%81%A1%E3%81%AF";
    REQUIRE(bell::uri::encode(input) == expected);
  }

  SECTION("Empty String") {
    std::string input;
    std::string expected;
    REQUIRE(bell::uri::encode(input) == expected);
  }
}

TEST_CASE("bell::uri::decode tests", "[bell::uri::decode]") {
  SECTION("Encoded Unreserved Characters") {
    std::string input =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    std::string expected = input;
    // Even if unreserved characters are percent-encoded, they should bell::uri::decode back to original
    std::string encoded = bell::uri::encode(input);
    REQUIRE(bell::uri::decode(encoded) == expected);
  }

  SECTION("Encoded Reserved Characters") {
    std::string input = "!*'();:@&=+$,/?#[]";
    std::string encoded =
        "%21%2A%27%28%29%3B%3A%40%26%3D%2B%24%2C%2F%3F%23%5B%5D";
    REQUIRE(bell::uri::decode(encoded) == input);
  }

  SECTION("Mixed Characters") {
    std::string input = "Hello%2C%20World%21";
    std::string expected = "Hello, World!";
    REQUIRE(bell::uri::decode(input) == expected);
  }

  SECTION("Non-ASCII Characters") {
    std::string input = "%E3%81%93%E3%82%93%E3%81%AB%E3%81%A1%E3%81%AF";
    std::string expected = "こんにちは";  // "Hello" in Japanese
    REQUIRE(bell::uri::decode(input) == expected);
  }

  SECTION("Empty String") {
    std::string input = "";
    std::string expected = "";
    REQUIRE(bell::uri::decode(input) == expected);
  }

  SECTION("Invalid Encoded Percent") {
    std::string input = "Invalid%2GInput";
    // Depending on your implementation, decide how to handle invalid sequences
    // For this test, we'll expect the '%' and following characters to be left as is
    std::string expected = "Invalid%2GInput";
    REQUIRE(bell::uri::decode(input) == expected);
  }

  SECTION("Incomplete Percent Encoding") {
    std::string input1 = "Incomplete%";
    std::string expected1 = "Incomplete%";
    REQUIRE(bell::uri::decode(input1) == expected1);

    std::string input2 = "Incomplete%2";
    std::string expected2 = "Incomplete%2";
    REQUIRE(bell::uri::decode(input2) == expected2);
  }

  SECTION("Plus Character") {
    std::string input = "A+plus+B";  // '+' should remain unchanged
    std::string expected = "A+plus+B";
    REQUIRE(bell::uri::decode(input) == expected);
  }

  SECTION("Encode-bell::uri::decode Round Trip") {
    std::string original =
        "Test String with special characters !*'();:@&=+$,/?#[]";
    std::string encoded = bell::uri::encode(original);
    std::string decoded = bell::uri::decode(encoded);
    REQUIRE(decoded == original);
  }

  SECTION("Upper and Lowercase Hex Digits") {
    std::string inputUpper = "%7E";
    std::string inputLower = "%7e";
    std::string expected = "~";
    REQUIRE(bell::uri::decode(inputUpper) == expected);
    REQUIRE(bell::uri::decode(inputLower) == expected);
  }
}