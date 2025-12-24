#pragma once

#include <sys/time.h>
#include <cstdint>

// Define an IRAM attribute macro for ESP platforms
// Does nothing on other platforms
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define BELL_IRAM_ATTR IRAM_ATTR
#else
#define BELL_IRAM_ATTR
#endif

// Contains various utility functions
namespace bell::utils {
// @brief Constructs a timeval struct from milliseconds
timeval millisecondsToTimeval(uint32_t milliseconds);

// @brief Converts a timeval struct to milliseconds
uint32_t timevalToMilliseconds(const timeval& tv);

// @brief Sleeps for the specified number of milliseconds
void sleepMs(uint32_t milliseconds);
}  // namespace bell::utils
