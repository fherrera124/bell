#pragma once

#include <cstdint>

namespace bell::utils {

/**
 * @brief Cross-platform high-resolution clock counter for performance profiling.
 * 
 * Uses platform-specific CPU cycle counters to measure elapsed time without being
 * affected by task interruptions (on ESP32) or with minimal overhead on desktop platforms.
 */
class ClockCounter {
 public:
  using Counter = uint64_t;

  /**
   * @brief Get the current clock counter value.
   * 
   * On ESP32: Returns CPU cycle count (not affected by task interruptions)
   * On desktop: Returns high-resolution monotonic clock
   * 
   * @return Current counter value
   */
  static Counter now();

  /**
   * @brief Convert counter ticks to microseconds.
   * 
   * @param ticks Number of clock ticks
   * @return Elapsed time in microseconds
   */
  static uint64_t toMicroseconds(Counter ticks);

  /**
   * @brief Convert counter ticks to milliseconds.
   * 
   * @param ticks Number of clock ticks
   * @return Elapsed time in milliseconds
   */
  static uint64_t toMilliseconds(Counter ticks);

  /**
   * @brief Calculate elapsed ticks between two counter values.
   * 
   * Handles counter overflow correctly.
   * 
   * @param start Starting counter value
   * @param end Ending counter value
   * @return Elapsed ticks
   */
  static Counter elapsed(Counter start, Counter end);
};

}  // namespace bell::utils
