#pragma once

#include <cstddef>
#include <limits>
#include <memory_resource>
#include <new>

#ifdef ESP_PLATFORM
#include "bell/utils/EspMemoryResource.h"
#endif

namespace bell::dsp {

#ifdef ESP_PLATFORM
/**
 * @brief Allocator that uses internal DRAM on ESP32 via polymorphic_allocator
 * 
 * This is a type alias for std::pmr::polymorphic_allocator configured to use
 * the internal memory resource. On ESP32, this forces allocations to internal
 * memory instead of PSRAM for better performance.
 */
template <typename T>
using InternalMemoryAllocator = std::pmr::polymorphic_allocator<T>;

/**
 * @brief Get the memory resource for internal DRAM allocations
 */
inline std::pmr::memory_resource* getInternalMemoryResource() {
  return &bell::utils::internalMemoryResource;
}

#else
/**
 * @brief Fallback to standard allocator on non-ESP32 platforms
 */
template <typename T>
using InternalMemoryAllocator = std::allocator<T>;

inline std::pmr::memory_resource* getInternalMemoryResource() {
  return std::pmr::get_default_resource();
}
#endif

}  // namespace bell::dsp
