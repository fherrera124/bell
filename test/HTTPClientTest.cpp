#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "bell/net/HTTPServer.h"
#include "bell/utils/Utils.h"

TEST_CASE("bell::http::Client tests", "[bell::http::Client]") {
  // TODO: Tests will be implemented along with the HTTPServer tests
  //
  auto server = std::make_unique<bell::net::HTTPServer>();

  server->listen(2137);
  bell::utils::sleepMs(120000);
}
