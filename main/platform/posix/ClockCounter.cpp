#include "bell/utils/ClockCounter.h"

#ifndef ESP_PLATFORM

#include <chrono>

using namespace bell::utils;

ClockCounter::Counter ClockCounter::now() {
  // Use high-resolution steady clock
  auto timePoint = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             timePoint.time_since_epoch())
      .count();
}

uint64_t ClockCounter::toMicroseconds(Counter ticks) {
  // Ticks are already in nanoseconds
  return ticks / 1000;
}

uint64_t ClockCounter::toMilliseconds(Counter ticks) {
  // Ticks are already in nanoseconds
  return ticks / 1000000;
}

ClockCounter::Counter ClockCounter::elapsed(Counter start, Counter end) {
  // Handle overflow case (unlikely with 64-bit counter)
  if (end >= start) {
    return end - start;
  } else {
    // Counter wrapped around
    return (UINT64_MAX - start) + end + 1;
  }
}

#endif  // !ESP_PLATFORM
