#pragma once

// Standard includes
#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_map>

// bell includes
#include "bell/audio/Types.h"

#ifndef BELL_DISABLE_TAOJSON
// Used for JSON deserialization of the transforms
#include <tao/json.hpp>
#endif

namespace bell::dsp {
// Holds the audio samples that are passed between the transforms in the pipeline.
struct DataSlots {
  // Maximum number of samples that can be stored in the slots.
  static const int maxSamples = 1024;
  static const int maxChannels = 8;

  // Per-channel sample storage.
  using ChannelMap = std::unordered_map<int, std::array<int32_t, maxSamples>>;

  // Internal storage for the audio samples.
  ChannelMap slotA{};
  ChannelMap slotB{};

  // Pointers to the primary and secondary slots.
  ChannelMap* primarySlot = &slotA;
  ChannelMap* secondarySlot = &slotB;

  /**
   * @brief Swaps data between the primary and secondary slot.
   */
  inline void swapSlots() { std::swap(primarySlot, secondarySlot); }

  /**
   * @brief Makes sure that the given channel exists in the slots.
   * 
   * @param channel channel index
   */
  inline void ensureChannel(int channel) const {
    if (primarySlot->find(channel) == primarySlot->end()) {
      primarySlot->emplace(channel, std::array<int32_t, maxSamples>{});
      secondarySlot->emplace(channel, std::array<int32_t, maxSamples>{});
    }
  }

  // Number of samples stored in the slots.
  size_t numSamples = 0;

  // Format of the samples stored in the slots.
  audio::Format sampleFormat;
};

// Base class for audio transforms, which process audio samples. For example, a transform could be an biquad filter, a reverb, or a compressor.
class Transform {
 public:
  Transform() = default;
  virtual ~Transform() = default;

  // Process the audio samples, modifying the samples in the slots.
  virtual void process(DataSlots& sampleSlots) = 0;

  // Calculate the required headroom for the transform. This is the amount of headroom required to prevent clipping in the output samples.
  virtual float calculateHeadroom() = 0;

  // Set the channels that the transform will affect.
  void setChannels(const std::vector<uint8_t>& channels);

 protected:
  std::recursive_mutex accessMutex;
  std::vector<uint8_t> channels{};
};

// Pipeline of transforms that audio samples are passed through. The pipeline processes the samples in order, with the output of each transform being the input to the next transform.
class TransformPipeline {
 public:
  TransformPipeline() = default;
  ~TransformPipeline() = default;

  // Add a transform to the pipeline.
  void addTransform(const std::shared_ptr<Transform>& transform);

  // Process the audio samples through the pipeline.
  void process(DataSlots& sampleSlots);

#ifndef BELL_DISABLE_TAOJSON
  // Create a TransformPipeline from a JSON description.
  static std::shared_ptr<TransformPipeline> fromJson(
      const tao::json::value& json);
#endif

 private:
  std::mutex accessMutex;
  std::vector<std::shared_ptr<Transform>> transforms{};
};
}  // namespace bell::dsp

namespace bell {
using TransformPipeline = dsp::TransformPipeline;
}
