#pragma once

// Standard includes
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

// bell includes
#include "bell/audio/Common.h"

namespace bell::dsp {
// Holds the audio samples that are passed between the transforms in the pipeline.
// Uses a slab allocator design for cache-friendly, contiguous memory layout.
class DataSlots {
 public:
  static constexpr size_t MAX_CHANNELS = 8;
  static constexpr size_t MAX_SAMPLES = 1024;

  // A single buffer holding audio data with slab allocation
  struct Slot {
    // Contiguous block of memory for ALL channels
    // Layout: [ Ch0_Samples... | Ch1_Samples... | Ch2_Samples... ]
    std::vector<int32_t> storage;

    // Fast pointers into 'storage' for each channel
    std::array<int32_t*, MAX_CHANNELS> channels;

    size_t numChannels = 0;
    size_t samplesPerChannel = 0;

    Slot() {
      // Initialize pointers to nullptr
      channels.fill(nullptr);
    }

    // Fast access to a channel's data
    inline int32_t* operator[](size_t channelIdx) { return channels[channelIdx]; }
    inline const int32_t* operator[](size_t channelIdx) const {
      return channels[channelIdx];
    }

    // Configure the slot for a specific number of channels and samples
    void configure(size_t channels, size_t samples) {
      if (channels > MAX_CHANNELS) {
        channels = MAX_CHANNELS;
      }
      if (samples > MAX_SAMPLES) {
        samples = MAX_SAMPLES;
      }

      // Only reallocate if size changed
      size_t requiredSize = channels * samples;
      if (storage.size() != requiredSize) {
        storage.resize(requiredSize);
      }

      numChannels = channels;
      samplesPerChannel = samples;

      // Set up channel pointers to point into the contiguous block
      for (size_t i = 0; i < channels; i++) {
        this->channels[i] = &storage[i * samples];
      }
      // Null out unused channel pointers
      for (size_t i = channels; i < MAX_CHANNELS; i++) {
        this->channels[i] = nullptr;
      }
    }

    // Helper for raw data access (for memcpy/memset)
    int32_t* data() { return storage.data(); }
    const int32_t* data() const { return storage.data(); }
  };

  // Actual Data Members
  Slot slotA;
  Slot slotB;

  // Pointers to the active/inactive slots (Ping-Pong)
  Slot* primarySlot;
  Slot* secondarySlot;

  audio::Format sampleFormat;
  size_t numSamples = 0;

  DataSlots() {
    primarySlot = &slotA;
    secondarySlot = &slotB;
  }

  // High-speed swap for pipeline stages
  inline void swapSlots() { std::swap(primarySlot, secondarySlot); }

  /**
   * @brief Configure the data slots for a specific audio format.
   * 
   * Allocates memory only when needed. Call this when the audio format changes.
   * 
   * @param samples Number of samples per channel
   * @param format Audio format (determines number of channels)
   */
  void configure(size_t samples, const audio::Format& format) {
    if (samples > MAX_SAMPLES) {
      samples = MAX_SAMPLES;
    }

    numSamples = samples;
    sampleFormat = format;

    size_t channels = format.getNumChannels();
    slotA.configure(channels, samples);
    slotB.configure(channels, samples);
  }
};

// Base class for audio transforms, which process audio samples. For example, a transform could be an biquad filter, a reverb, or a compressor.
class Transform {
 public:
  Transform() = default;
  virtual ~Transform() = default;

  // Enumeration of standard transform types.
  // Custom transform types can be added by subclassing Transform.
  enum class Type { GAIN, MIXER, BIQUAD, OTHER };

  // Process the audio samples, modifying the samples in the slots.
  virtual void process(DataSlots& sampleSlots) = 0;

  // Calculate the required headroom for the transform. This is the amount of headroom required to prevent clipping in the output samples.
  virtual float calculateHeadroom() = 0;

  // Update the sample rate of the transform.
  virtual void sampleRateUpdated(const audio::SampleRate sampleRate) {
    std::scoped_lock lock(accessMutex);
    this->sampleRate = sampleRate;
  }

  virtual Type getType() const { return Type::OTHER; }

  // Set the channels that the transform will affect.
  void setChannels(const std::vector<int>& channels);

  // Return the channels that the transform will affect.
  std::vector<int> getChannels() const { return channels; }

 protected:
  std::recursive_mutex accessMutex;
  std::vector<int> channels{};
  audio::SampleRate sampleRate = audio::SampleRate::SR_44100HZ;
};

// Pipeline of transforms that audio samples are passed through. The pipeline processes the samples in order, with the output of each transform being the input to the next transform.
class TransformPipeline {
 public:
  TransformPipeline() = default;
  ~TransformPipeline() = default;

  // Add a transform to the pipeline.
  void addTransform(const std::shared_ptr<Transform>& transform);

  void addTransforms(const std::vector<std::shared_ptr<Transform>>& transforms);

  // Process the audio samples through the pipeline.
  void process(DataSlots& sampleSlots);

 private:
  std::mutex accessMutex;
  std::optional<audio::SampleRate> lastSampleRate{};
  std::vector<std::shared_ptr<Transform>> transforms{};

#ifdef BELL_DSP_ENABLE_PROFILING
  // Profiling state (only compiled when profiling is enabled)
  struct TransformStats {
    uint64_t totalCycles = 0;
    uint64_t callCount = 0;
  };
  std::vector<TransformStats> transformStats{};
  uint64_t lastLogTime = 0;
  static constexpr uint64_t LOG_INTERVAL_MS = 5000;  // Log every 5 seconds
#endif
};
}  // namespace bell::dsp

namespace bell {
using TransformPipeline = dsp::TransformPipeline;
}
