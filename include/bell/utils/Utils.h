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

// @brief Opaque identity of the calling thread / FreeRTOS task.
//
// Returns a stable uint64_t that compares equal iff the caller is the
// same thread of execution across calls. Safe to use for thread-affinity
// asserts from any callsite, regardless of how the thread was created.
//
// Rationale: std::this_thread::get_id() routes through pthread_self() in
// ESP-IDF's libstdc++. Tasks created via xTaskCreate (as bell::Task does)
// don't go through pthread_create, so pthread_self() may return a default
// / "not-a-thread" value — causing std::thread::id comparisons to false-
// match across different tasks. Using xTaskGetCurrentTaskHandle directly
// avoids that entire class of bug on ESP.
//
// Returned value: non-zero on a live thread; 0 is reserved as "not set".
uint64_t currentThreadId();
}  // namespace bell::utils
