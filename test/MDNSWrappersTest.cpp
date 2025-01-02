#include <unistd.h>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <mutex>
#include <thread>

#include "bell/mdns/Browser.h"
#include "bell/mdns/Service.h"
#include "bell/utils/Utils.h"

namespace {
// Set to false to stop the browser runner thread
std::atomic<bool> mdnsBrowserRunning = false;
std::mutex mdnsBrowserMutex;

// Runs the MDNS browser update loop in a separate thread, till mdnsBrowserRunning is false
void runDiscoveryLoop(bell::mdns::Browser* browser) {
  std::scoped_lock lock(mdnsBrowserMutex);
  mdnsBrowserRunning = true;

  while (mdnsBrowserRunning) {
    browser->processEvents(100);
  }
}
}  // namespace

TEST_CASE("bell::mdns tests", "[bell::mdns]") {
  std::string serviceName = "mdns-discovery-test";
  std::atomic<bool> serviceAdded = false;
  std::atomic<bool> serviceAddrResolved = false;
  std::atomic<bool> serviceRemoved = false;

  auto browser = bell::mdns::Browser::startDiscovery(
      "_bell._tcp", "", 0,
      [&serviceName, &serviceAdded, &serviceAddrResolved, &serviceRemoved](
          const auto& eventType, const auto& service) {
        // Save up events for the requested service
        if (service.name == serviceName) {
          switch (eventType) {
            case bell::mdns::EventType::ServiceAdded:
              serviceAdded = true;
              break;
            case bell::mdns::EventType::AddressResolved:
              serviceAddrResolved = true;
              break;
            case bell::mdns::EventType::ServiceRemoved:
              serviceRemoved = true;
              break;
            default:
              break;
          }
        }
      });

  std::thread mdnsBrowserRunner(runDiscoveryLoop, browser.get());

  SECTION("Properly registers and unregisters a service") {
    REQUIRE_FALSE(serviceAdded);
    REQUIRE_FALSE(serviceAddrResolved);
    REQUIRE_FALSE(serviceRemoved);

    {
      auto service = bell::mdns::Service::registerService(
          serviceName, "_bell", "_tcp", "", 1234,
          {{"dupa", "value"}, {"dupa2", "value2"}});
      bell::utils::sleepMs(1000);

      // Service should be added and addr resolved by now
      REQUIRE(serviceAdded == true);
      REQUIRE(serviceAddrResolved == true);

      // Service should not be removed yet
      REQUIRE_FALSE(serviceRemoved);
    }
    bell::utils::sleepMs(2000);

    // Service should be removed by now
    REQUIRE(serviceRemoved);
  }

  {
    // Stop the browser
    mdnsBrowserRunning = false;
    std::scoped_lock lock(mdnsBrowserMutex);
    mdnsBrowserRunner.join();
  }
}
