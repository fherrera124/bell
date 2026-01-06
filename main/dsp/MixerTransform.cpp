#include "bell/dsp/MixerTransform.h"

#include "IQmathLib.h"

#include <cstring>

using namespace bell;

void dsp::MixerTransform::configure(
    const std::vector<std::vector<int>>& mixerMapping) {
  std::scoped_lock lock(accessMutex);
  this->mixerMapping = mixerMapping;
}

float dsp::MixerTransform::calculateHeadroom() {
  return 0.0F;  // No headroom required for mixer
}

void dsp::MixerTransform::process(dsp::DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  const size_t numSamples = sampleSlots.numSamples;

  for (size_t outputChannelIdx = 0;
       outputChannelIdx < this->mixerMapping.size(); outputChannelIdx++) {
    const auto& inputChannels = mixerMapping[outputChannelIdx];

    // Count valid input channels first
    int numValidInputs = 0;
    for (int chanIdx : inputChannels) {
      if (chanIdx < static_cast<int>(sampleSlots.primarySlot->numChannels) &&
          (*sampleSlots.primarySlot)[chanIdx] != nullptr) {
        numValidInputs++;
      }
    }

    int32_t* outputChanData = (*sampleSlots.secondarySlot)[outputChannelIdx];

    // Early exit if no valid inputs
    if (numValidInputs == 0) {
      memset(outputChanData, 0, sizeof(int32_t) * numSamples);
      continue;
    }

    // Pre-calculate scale factor as IQ30 value to use multiplication instead of division
    // For 1 input: scale = 1.0 (no scaling needed)
    // For 2 inputs: scale = 0.5 = 2^29 in IQ30
    // For 3 inputs: scale = 0.333... ≈ 357913941 in IQ30
    // For N inputs: scale = 1/N in IQ30 format
    int32_t scaleFactor = 0;
    bool needsScaling = (numValidInputs > 1);

    if (needsScaling) {
      if (numValidInputs == 2) {
        scaleFactor = (1 << 29);  // 0.5 in IQ30 = 2^29
      } else {
        // For 3+ inputs, calculate 1/N in IQ30 format
        // (1 << 30) / N gives us 1/N in IQ30
        scaleFactor = (1 << 30) / numValidInputs;
      }
    }

    // Initialize output to zero
    memset(outputChanData, 0, sizeof(int32_t) * numSamples);

    // Accumulate all input channels
    for (int chanIdx : inputChannels) {
      // Validate channel
      if (chanIdx >= static_cast<int>(sampleSlots.primarySlot->numChannels)) {
        continue;
      }

      int32_t* inputChanData = (*sampleSlots.primarySlot)[chanIdx];
      if (inputChanData == nullptr) {
        continue;
      }

      // Accumulate samples
      for (size_t i = 0; i < numSamples; i++) {
        outputChanData[i] += inputChanData[i];
      }
    }

    // Apply downmixing scale if needed
    if (needsScaling) {
      for (size_t i = 0; i < numSamples; i++) {
        outputChanData[i] = _IQ30mpy(outputChanData[i], scaleFactor);
      }
    }
  }

  sampleSlots.swapSlots();  // Swap the DSP buffers
}
