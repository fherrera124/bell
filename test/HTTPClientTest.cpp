#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>

#include <sys/socket.h>
#include "bell/io/HTTPClient.h"

TEST_CASE("bell::http::Client tests", "[bell::http::Client]") {
  SECTION("Client can connect to a server") {
    auto conn = std::make_unique<bell::http::Connection>(
        "http://ip.jsontest.com/", 1000);
    conn->sendRequest("GET", {}, 0);
    auto response = conn->getResponse();

  }
}