#include <unistd.h>
#include <catch2/catch_test_macros.hpp>

#include "bell/net/IpAddress.h"

TEST_CASE("bell::net::IpAddress tests", "[bell::net::IpAddress]") {
  SECTION("resolveDomain properly resolves domains") {
    auto resolved =
        bell::net::IpAddress::resolveDomain("localhost", SOCK_STREAM);

    // Localhost should resolve to either IPv4 or IPv6
    REQUIRE((resolved.getType() != bell::net::IpAddress::Type::Unknown));
    REQUIRE(resolved.getSockAddrLen() > 0);

    // Requesting IPv4 should return IPv4
    resolved =
        bell::net::IpAddress::resolveDomain("localhost", SOCK_STREAM, AF_INET);

    // Should be IPv4
    REQUIRE((resolved.getType() == bell::net::IpAddress::Type::IPv4));

    // Resolving a known domain should return a valid address
    resolved = bell::net::IpAddress::resolveDomain("google.com", SOCK_STREAM);
    REQUIRE((resolved.getType() != bell::net::IpAddress::Type::Unknown));
    REQUIRE(resolved.getSockAddrLen() > 0);

    // Resolving an invalid domain should fail
    REQUIRE_THROWS(
        bell::net::IpAddress::resolveDomain("_invalid.domai", SOCK_STREAM));
  }

  SECTION("resolveDomain properly copies IP addresses") {
    auto resolved =
        bell::net::IpAddress::resolveDomain("127.0.0.1", SOCK_STREAM);

    // Should be IPv4
    REQUIRE(resolved.getType() == bell::net::IpAddress::Type::IPv4);
    REQUIRE(resolved.getSockAddrLen() > 0);

    // Throws on invalid address
    REQUIRE_THROWS(bell::net::IpAddress::resolveDomain("124.1.", SOCK_STREAM));

    // Resolves IPv6
    resolved = bell::net::IpAddress::resolveDomain("::1", SOCK_STREAM);
    REQUIRE(resolved.getType() == bell::net::IpAddress::Type::IPv6);
  }
}

TEST_CASE("bell::io::Socket and derieved classes tests", "[bell::io::Socket]") {
}
