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

  switch (format.getBitWidth()) {
    case audio::BitWidth::BW_16: {
      for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
           frameIdx++) {
        for (auto chan = 0; chan < numChannels; chan++) {
          innerDataSlots.sampleSlots.at(chan)[frameIdx] =
              inputAsInt16[(frameIdx * numChannels) + chan]
              << 15U;  // Shift left by 15 bits to roughly 32bit range
        }
      }
      break;
    }

    case audio::BitWidth::BW_32: {
      for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
           frameIdx++) {
        for (auto chan = 0; chan < numChannels; chan++) {
          // No need to shift left by 15 bits, as the input is already in 32bit range
          innerDataSlots.sampleSlots.at(chan)[frameIdx] =
              inputAsInt32[(frameIdx * numChannels) + chan];
        }
      }
      break;
    }

    default:
      // Unsupported bit width
      throw std::runtime_error("Unsupported bit width");
      break;
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

  switch (format.getBitWidth()) {
    case audio::BitWidth::BW_16: {
      for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
           frameIdx++) {
        for (auto chan = 0; chan < numChannels; chan++) {
          // Shift right by 15 bits to convert back to 16bit range
          outputData16[(frameIdx * numChannels) + chan] =
              innerDataSlots.sampleSlots.at(chan)[frameIdx] >> 15U;
        }
      }
      break;
    }

    case audio::BitWidth::BW_32: {
      for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
           frameIdx++) {
        for (auto chan = 0; chan < numChannels; chan++) {
          // Shift right by 15 bits to convert back to 16bit range
          outputData32[(frameIdx * numChannels) + chan] =
              innerDataSlots.sampleSlots.at(chan)[frameIdx];
        }
      }
      break;
    }

    default:
      // Unsupported bit width
      throw std::runtime_error("Unsupported bit width");
      break;
  }

  return &innerDataSlots;
}