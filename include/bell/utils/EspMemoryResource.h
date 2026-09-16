#pragma once

#include "bell/Logger.h"
#ifdef ESP_PLATFORM

#include <memory_resource>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace bell::utils {

// std::pmr implementation for ESP-IDF using heap capabilities
template <uint32_t Caps>
class EspMemoryResource : public std::pmr::memory_resource {
 public:
  EspMemoryResource() = default;
  ~EspMemoryResource() override = default;

  EspMemoryResource(const EspMemoryResource&) = delete;
  EspMemoryResource& operator=(const EspMemoryResource&) = delete;

 protected:
  void* do_allocate(size_t bytes, size_t alignment) override {
    void* ptr = heap_caps_aligned_alloc(alignment, bytes, Caps);

    // std::pmr::memory_resource's contract is to throw, which lets a
    // caller degrade instead of taking the whole device down.
    if (ptr == nullptr) {
      BELL_LOG(error, "EspMemoryResource",
               "OOM! Failed to allocate {} bytes (align {}) with caps 0x{:x}",
               bytes, alignment, (unsigned long)Caps);
      throw std::bad_alloc();
    }

    return ptr;
  }

  void do_deallocate(void* p, size_t bytes, size_t alignment) override {
    (void)bytes;
    (void)alignment;
    heap_caps_free(p);
  }

  bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
};

// Prefers Caps and takes FallbackCaps when the preferred pool is empty, so
// a tight internal heap costs speed rather than the feature. The first
// fallback is logged: it is a performance change worth seeing.
template <uint32_t Caps, uint32_t FallbackCaps>
class EspFallbackMemoryResource : public std::pmr::memory_resource {
 public:
  EspFallbackMemoryResource() = default;
  ~EspFallbackMemoryResource() override = default;

  EspFallbackMemoryResource(const EspFallbackMemoryResource&) = delete;
  EspFallbackMemoryResource& operator=(const EspFallbackMemoryResource&) =
      delete;

 protected:
  void* do_allocate(size_t bytes, size_t alignment) override {
    void* ptr = heap_caps_aligned_alloc(alignment, bytes, Caps);
    if (ptr != nullptr) {
      return ptr;
    }

    ptr = heap_caps_aligned_alloc(alignment, bytes, FallbackCaps);
    if (ptr == nullptr) {
      BELL_LOG(error, "EspMemoryResource",
               "OOM! Failed to allocate {} bytes (align {}) with caps 0x{:x} "
               "or 0x{:x}",
               bytes, alignment, (unsigned long)Caps,
               (unsigned long)FallbackCaps);
      throw std::bad_alloc();
    }

    if (!fallbackLogged) {
      fallbackLogged = true;
      BELL_LOG(warn, "EspMemoryResource",
               "caps 0x{:x} exhausted; {} bytes served from 0x{:x} instead",
               (unsigned long)Caps, bytes, (unsigned long)FallbackCaps);
    }
    return ptr;
  }

  void do_deallocate(void* p, size_t bytes, size_t alignment) override {
    (void)bytes;
    (void)alignment;
    heap_caps_free(p);
  }

  bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

 private:
  bool fallbackLogged = false;
};

using DmaMemoryResource =
    EspMemoryResource<MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT>;

using InternalMemoryResource =
    EspMemoryResource<MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT>;

using PsramMemoryResource =
    EspMemoryResource<MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT>;

// Static instance of the DMA memory resource
static DmaMemoryResource dmaMemoryResource{};

// Static instance of the internal memory resource
static InternalMemoryResource internalMemoryResource{};

// Static instance of the PSRAM memory resource
static PsramMemoryResource psramMemoryResource{};

using InternalThenPsramMemoryResource =
    EspFallbackMemoryResource<MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT>;

static InternalThenPsramMemoryResource internalThenPsramMemoryResource{};

}  // namespace bell::utils

#endif  // ESP_PLATFORM
