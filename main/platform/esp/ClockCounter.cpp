#include "bell/utils/ClockCounter.h"

#ifdef ESP_PLATFORM

#include <esp_clk_tree.h>
#include <esp_cpu.h>
#include <freertos/FreeRTOS.h>

using namespace bell::utils;

ClockCounter::Counter ClockCounter::now() {
  // Get the CPU cycle count - this is not affected by task switching
  return esp_cpu_get_cycle_count();
}

uint64_t ClockCounter::toMicroseconds(Counter ticks) {
  // ESP32 CPU frequency in MHz
  uint32_t cpuFreqHz = 0;
  esp_clk_tree_src_get_freq_hz(
      SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpuFreqHz);
  uint32_t cpuFreqMHz = cpuFreqHz / 1000000;
  return ticks / cpuFreqMHz;
}

uint64_t ClockCounter::toMilliseconds(Counter ticks) {
  return toMicroseconds(ticks) / 1000;
}

ClockCounter::Counter ClockCounter::elapsed(Counter start, Counter end) {
  // Handle overflow case
  if (end >= start) {
    return end - start;
  } else {
    // Counter wrapped around
    return (UINT64_MAX - start) + end + 1;
  }
}

#endif  // ESP_PLATFORM
