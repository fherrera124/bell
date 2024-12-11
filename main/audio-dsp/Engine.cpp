#include "bell/dsp/Engine.h"
#include "bell/audio/Types.h"

using namespace bell::dsp;

void Engine::applyPipeline(const std::shared_ptr<TransformPipeline>& pipeline) {
  // Replace the active pipeline with the new one
  std::scoped_lock lock(accessMutex);
  activePipeline = pipeline;
}

DataSlots* Engine::process(const uint8_t* inputBuffer, size_t inputBufferLen,
                           uint8_t* outputBuffer, size_t outputBufferLen,
                           const audio::Format& format) {
  std::scoped_lock lock(accessMutex);

  // Check if a pipeline is set
  if (!activePipeline) {
    return nullptr;
  }

  // Check if all the channels are set
  for (int x = 0; x < format.getNumChannels(); x++) {
    if (innerDataSlots.sampleSlots.find(x) ==
        innerDataSlots.sampleSlots.end()) {
      innerDataSlots.sampleSlots[x] = {};
    }
  }

  if (innerDataSlots.sampleFormat != format) {
    // Update the format of the inner data slots, and clear the slots
    innerDataSlots.sampleFormat = format;
  }

  innerDataSlots.numSamples = format.bytesToSamples(inputBufferLen);

  const auto* inputAsInt16 = reinterpret_cast<const int16_t*>(inputBuffer);
  const auto* inputAsInt32 = reinterpret_cast<const int32_t*>(inputBuffer);

  uint8_t numChannels = format.getNumChannels();

  // Normalize data into floats, for processing
  for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples; frameIdx++) {
    for (auto chan = 0; chan < numChannels; chan++) {
      switch (innerDataSlots.sampleFormat.getBitWidth()) {
        case audio::BitWidth::BW_16: {
          innerDataSlots.sampleSlots.at(chan)[frameIdx] =
              static_cast<float>(inputAsInt16[(frameIdx * numChannels) + chan] /
                                 (float)std::numeric_limits<int16_t>::max());
          break;
        }
        case audio::BitWidth::BW_32: {
          innerDataSlots.sampleSlots.at(chan)[frameIdx] =
              inputAsInt32[(frameIdx * numChannels) + chan] /
              (float)std::numeric_limits<int32_t>::max();
          break;
        }
        default:
          // Unsupported bit width
          throw std::runtime_error("Unsupported bit width");
          break;
      }
    }
  }

  // Process the samples
  activePipeline->process(innerDataSlots);

  // Check if the output buffer is large enough
  if (format.samplesToBytes(innerDataSlots.numSamples) > outputBufferLen) {
    throw std::runtime_error("Output buffer is too small");
    return nullptr;
  }

  auto* outputData16 = reinterpret_cast<int16_t*>(outputBuffer);
  auto* outputData32 = reinterpret_cast<int32_t*>(outputBuffer);

  // Cache common properties
  const audio::BitWidth bitWidth = innerDataSlots.sampleFormat.getBitWidth();
  const float clipMax = 1.0F;

  // Validate bit width beforehand
  if (bitWidth != audio::BitWidth::BW_16 &&
      bitWidth != audio::BitWidth::BW_32) {
    throw std::runtime_error("Unsupported bit width in the output format");
  }

  // Denormalize frames back into PCM data
  for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples; ++frameIdx) {
    for (uint8_t chan = 0; chan < numChannels; ++chan) {
      // Retrieve and clip sample
      float sample = std::clamp(innerDataSlots.sampleSlots.at(chan)[frameIdx],
                                -clipMax, clipMax);

      // Calculate output index
      const size_t outputIdx = (frameIdx * numChannels) + chan;

      // Write data based on bit width
      if (bitWidth == audio::BitWidth::BW_16) {
        outputData16[outputIdx] =
            static_cast<int16_t>(sample * std::numeric_limits<int16_t>::max());
      } else if (bitWidth == audio::BitWidth::BW_32) {
        outputData32[outputIdx] = static_cast<int32_t>(
            sample * static_cast<float>(std::numeric_limits<int32_t>::max()));
      }
    }
  }

  return &innerDataSlots;
}