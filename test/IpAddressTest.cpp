#include <unistd.h>
#include <catch2/catch_test_macros.hpp>

#include "bell/net/IpAddress.h"

TEST_CASE("bell::net::IpAddress tests", "[bell::net::IpAddress]") {
  SECTION("resolveDomain properly resolves domains") {
    auto result = bell::net::IpAddress::resolveDomain("localhost", SOCK_STREAM);

    // Localhost should resolve to either IPv4 or IPv6
    REQUIRE(result.isSuccess());

    auto resolved = result.getValue();
    REQUIRE((resolved.getType() != bell::net::IpAddress::Type::Unknown));
    REQUIRE(resolved.getSockAddrLen() > 0);

    // Requesting IPv4 should return IPv4
    result =
        bell::net::IpAddress::resolveDomain("localhost", SOCK_STREAM, AF_INET);
    REQUIRE(result.isSuccess());

    resolved = result.getValue();

    // Should be IPv4
    REQUIRE((resolved.getType() == bell::net::IpAddress::Type::IPv4));

    // Resolving an invalid domain should fail
    result = bell::net::IpAddress::resolveDomain("_invalid.domai", SOCK_STREAM);

    REQUIRE(!result.isSuccess());
  }

  SECTION("resolveDomain properly copies IP addresses") {
    auto result = bell::net::IpAddress::resolveDomain("127.0.0.1", SOCK_STREAM);

    REQUIRE(result.isSuccess());

    auto resolved = result.getValue();

    // Should be IPv4
    REQUIRE(resolved.getType() == bell::net::IpAddress::Type::IPv4);
    REQUIRE(resolved.getSockAddrLen() > 0);

    result = bell::net::IpAddress::resolveDomain("124.1.", SOCK_STREAM);
    REQUIRE(!result.isSuccess());

    // Resolves IPv6
    result = bell::net::IpAddress::resolveDomain("::1", SOCK_STREAM);

    REQUIRE(result.isSuccess());

    resolved = result.getValue();
    REQUIRE(resolved.getType() == bell::net::IpAddress::Type::IPv6);
  }
}
