#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>

namespace bell::io {

// Thread safe SPSC ring buffer. Optimized for single producer and single consumer.
// Uses atomic operations for synchronization without mutexes.
template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity)
      : capacityValue(capacity), mask(capacity - 1), buffer(new T[capacity]) {
    // HARD REQUIREMENT: Capacity must be a power of 2
    // Checks if (X > 0) AND ((X & (X-1)) == 0)
    assert(capacity > 0 && ((capacity & (capacity - 1)) == 0) &&
           "Capacity must be power of 2");

    head.store(0, std::memory_order_relaxed);
    tail.store(0, std::memory_order_relaxed);
  }

  ~RingBuffer() = default;

  // Delete copy constructor and assignment
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  /**
   * @brief Attempts to push an element using move semantics (producer side)
   *
   * @param item Item to push (will be moved)
   * @return true if successfully pushed, false if buffer is full
   */
  bool push(T&& item) {
    const size_t currentTail = tail.load(std::memory_order_relaxed);
    const size_t nextTail = (currentTail + 1) & mask;

    if (nextTail == head.load(std::memory_order_acquire)) {
      return false;
    }

    buffer[currentTail] = std::move(item);
    tail.store(nextTail, std::memory_order_release);
    return true;
  }

  // Variant of the push method that uses copy semantics
  bool push(const T& item) {
    const size_t currentTail = tail.load(std::memory_order_relaxed);
    const size_t nextTail = (currentTail + 1) & mask;

    if (nextTail == head.load(std::memory_order_acquire)) {
      return false;  // Buffer is full
    }

    buffer[currentTail] = item;  // Copy assignment
    tail.store(nextTail, std::memory_order_release);
    return true;
  }

  /**
   * @brief Attempts to pop an element from the buffer (consumer side)
   *
   * @return Optional containing the element if available, nullopt if buffer is empty
   */
  std::optional<T> pop() {
    const size_t currentHead = head.load(std::memory_order_relaxed);

    if (currentHead == tail.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    T item = std::move(buffer[currentHead]);
    head.store((currentHead + 1) & mask, std::memory_order_release);
    return item;
  }

  // Checks if the buffer is empty (consumer side)
  bool isEmpty() const {
    return head.load(std::memory_order_acquire) ==
           tail.load(std::memory_order_acquire);
  }

  // Checks if the buffer is full (producer side)
  bool isFull() const {
    const size_t currentTail = tail.load(std::memory_order_acquire);
    const size_t nextTail = increment(currentTail);
    return nextTail == head.load(std::memory_order_acquire);
  }

  /**
   * @brief Returns the approximate number of elements in the buffer
   *
   * Note: This is an estimate and may not be exact due to concurrent operations
   *
   * @return Approximate size
   */
  size_t size() const {
    const size_t currentHead = head.load(std::memory_order_acquire);
    const size_t currentTail = tail.load(std::memory_order_acquire);

    if (currentTail >= currentHead) {
      return currentTail - currentHead;
    }
    return capacityValue - currentHead + currentTail;
  }

  // Return max capacity of the buffer
  size_t capacity() const {
    return capacityValue - 1;
  }  // -1 because one slot is always reserved

  /**
   * @brief Clears all elements from the buffer (consumer side only)
   *
   * WARNING: Should only be called when producer is stopped
   */
  void clear() {
    while (pop().has_value()) {
      // Drain the buffer
    }
  }

 private:
  // Get the cache line size for current platform
  static constexpr size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  const size_t capacityValue;
  const size_t mask;
  std::unique_ptr<T[]> buffer;

  // Align the variable directly.
  alignas(cacheLineSize) std::atomic<size_t> head;
  alignas(cacheLineSize) std::atomic<size_t> tail;

  // Increments an index with wraparound
  size_t increment(size_t idx) const { return (idx + 1) % capacityValue; }
};

}  // namespace bell::io

namespace bell {
using io::RingBuffer;
}  // namespace bell
