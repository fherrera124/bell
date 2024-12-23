#include <unistd.h>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "bell/utils/Semaphore.h"

TEST_CASE("bell::utils::Semaphore basic functionality",
          "[bell::utils::Semaphore]") {
  bell::utils::Semaphore sem(1);

  SECTION("Takes without blocking when semaphore is available") {
    REQUIRE_NOTHROW(sem.take());
  }

  SECTION("blocks when not available") {
    sem.take();

    // Start a thread that tries to take the semaphore without waiting
    std::thread testThread([&]() {
      auto start = std::chrono::steady_clock::now();
      // Try to take the semaphore with a timeout
      sem.take(1000);
      auto end = std::chrono::steady_clock::now();
      auto diff =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
              .count();

      REQUIRE(diff >= 1000);  // Should have timed out
    });

    testThread.join();
  }
}
