#pragma once

#include <sys/time.h>
#include <cstdint>

// Contains various utility functions
namespace bell::utils {
// @brief Constructs a timeval struct from milliseconds
timeval millisecondsToTimeval(uint32_t milliseconds);
}  // namespace bell::utils
