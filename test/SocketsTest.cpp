#include <unistd.h>
#include <catch2/catch_test_macros.hpp>

#include <sys/socket.h>
#include "bell/io/SocketUtils.h"

TEST_CASE("bell::io::SocketUtils tests", "[bell::io::SocketUtils]") {
  SECTION("resolveDomain properly resolves domains") {
    auto resolved =
        bell::io::SocketUtils::resolveDomain("localhost", SOCK_STREAM);

    // Localhost should resolve to either IPv4 or IPv6
    REQUIRE((resolved.family == AF_INET || resolved.family == AF_INET6));
    REQUIRE(resolved.addrLen > 0);

    // Requesting IPv4 should return IPv4
    resolved =
        bell::io::SocketUtils::resolveDomain("localhost", SOCK_STREAM, AF_INET);

    // Should be IPv4
    REQUIRE(resolved.family == AF_INET);
    REQUIRE(resolved.addrLen > 0);

    // Resolving a known domain should return a valid address
    resolved = bell::io::SocketUtils::resolveDomain("google.com", SOCK_STREAM);
    REQUIRE((resolved.family == AF_INET || resolved.family == AF_INET6));
    REQUIRE(resolved.addrLen > 0);

    // Resolving an invalid domain should fail
    REQUIRE_THROWS(
        bell::io::SocketUtils::resolveDomain("_invalid.domai", SOCK_STREAM));
  }

  SECTION("SocketUtils::resolveDomain properly copies IP addresses") {
    auto resolved =
        bell::io::SocketUtils::resolveDomain("127.0.0.1", SOCK_STREAM);

    // Should be IPv4
    REQUIRE(resolved.family == AF_INET);
    REQUIRE(resolved.addrLen > 0);

    // Throws on invalid address
    REQUIRE_THROWS(bell::io::SocketUtils::resolveDomain("124.1.", SOCK_STREAM));

    // Resolves IPv6
    resolved = bell::io::SocketUtils::resolveDomain("::1", SOCK_STREAM);
    REQUIRE(resolved.family == AF_INET6);
  }
}

TEST_CASE("bell::io::Socket and derieved classes tests", "[bell::io::Socket]") {
}