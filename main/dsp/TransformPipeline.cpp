#include "bell/dsp/TransformPipeline.h"

// Enable profiling with -DBELL_DSP_ENABLE_PROFILING
#ifdef BELL_DSP_ENABLE_PROFILING
#include "bell/Logger.h"
#include "bell/utils/ClockCounter.h"
#endif

#include "bell/utils/Utils.h"

#ifndef BELL_DISABLE_TAOJSON
// Used for JSON deserialization of the transforms
#include <tao/json.hpp>
#include <tao/json/contrib/traits.hpp>
#endif

using namespace bell::dsp;

void Transform::setChannels(const std::vector<int>& channels) {
  std::scoped_lock lock(accessMutex);
  this->channels = channels;
}

void TransformPipeline::addTransform(
    const std::shared_ptr<Transform>& transform) {
  std::scoped_lock lock(accessMutex);
  transforms.push_back(transform);
#ifdef BELL_DSP_ENABLE_PROFILING
  transformStats.push_back(TransformStats{});
#endif
}

void TransformPipeline::process(DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  // Check if the sample rate has been updated
  bool sampleRateUpdated =
      !lastSampleRate.has_value() ||
      lastSampleRate.value() != sampleSlots.sampleFormat.getSampleRate();
  lastSampleRate = sampleSlots.sampleFormat.getSampleRate();

  // Process the samples through each transform in the pipeline
  for (size_t i = 0; i < transforms.size(); i++) {
    const auto& transform = transforms[i];

    if (sampleRateUpdated) {
      // Notify the transform of the updated sample rate
      transform->sampleRateUpdated(sampleSlots.sampleFormat.getSampleRate());
    }

#ifdef BELL_DSP_ENABLE_PROFILING
    // Profile transform execution
    auto startTime = bell::utils::ClockCounter::now();
#endif
    transform->process(sampleSlots);
#ifdef BELL_DSP_ENABLE_PROFILING
    auto endTime = bell::utils::ClockCounter::now();

    // Update statistics
    auto elapsed = bell::utils::ClockCounter::elapsed(startTime, endTime);
    transformStats[i].totalCycles += elapsed;
    transformStats[i].callCount++;
#endif
  }

#ifdef BELL_DSP_ENABLE_PROFILING
  // Periodic logging of profiling results
  auto currentTime = bell::utils::ClockCounter::now();
  uint64_t timeSinceLastLog = bell::utils::ClockCounter::toMilliseconds(
      bell::utils::ClockCounter::elapsed(lastLogTime, currentTime));

  if (lastLogTime == 0 || timeSinceLastLog >= LOG_INTERVAL_MS) {
    lastLogTime = currentTime;

    BELL_LOG(info, "DSP", "Transform Pipeline Profiling:");
    for (size_t i = 0; i < transforms.size(); i++) {
      if (transformStats[i].callCount > 0) {
        uint64_t avgCycles =
            transformStats[i].totalCycles / transformStats[i].callCount;
        uint64_t avgUs = bell::utils::ClockCounter::toMicroseconds(avgCycles);
        BELL_LOG(info, "DSP",
                 "  Transform {}: avg={} us, calls={}, total={} ms", i, avgUs,
                 transformStats[i].callCount,
                 bell::utils::ClockCounter::toMilliseconds(
                     transformStats[i].totalCycles));
      }
    }

    // Reset statistics after logging
    for (auto& stats : transformStats) {
      stats.totalCycles = 0;
      stats.callCount = 0;
    }
  }
#endif
}
