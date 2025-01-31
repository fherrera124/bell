#include "bell/dsp/MixerTransform.h"

using namespace bell;

void dsp::MixerTransform::configure(
    const std::vector<std::pair<int, int>>& mixerMapping) {
  std::scoped_lock lock(accessMutex);
  this->mixerMapping = mixerMapping;
}

float dsp::MixerTransform::calculateHeadroom() {
  return 0.0F;  // No headroom required for mixer
}

void dsp::MixerTransform::process(dsp::DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  // Initialize each target channel in sampleSlots to make sure we do not run into missing keys
  for (const auto& [inputChannel, outputChannel] : mixerMapping) {
    // Clear the output data acc
    outputDataAcc[outputChannel].first.fill(0.0F);
    outputDataAcc[outputChannel].second = 0;

    if (sampleSlots.sampleSlots.find(outputChannel) ==
        sampleSlots.sampleSlots.end()) {
      sampleSlots.sampleSlots[outputChannel] = ChannelData{};
    }
  }

  // Fill each outputChannel with contributions from inputChannels
  for (const auto& [inputChannel, outputChannel] : mixerMapping) {
    const auto itInput = sampleSlots.sampleSlots.find(inputChannel);
    if (itInput != sampleSlots.sampleSlots.end()) {
      auto& [dataSum, count] = outputDataAcc[outputChannel];
      auto& inputData = itInput->second;
      for (size_t i = 0; i < sampleSlots.numSamples; ++i) {
        dataSum[i] += inputData[i];
      }
      count += 1;
    }
  }

  // Write back the accumulated data to sampleSlots, averaging if necessary
  for (auto& [outputChannel, acc] : outputDataAcc) {
    auto& [dataSum, count] = acc;
    auto& outputData = sampleSlots.sampleSlots[outputChannel];
    for (size_t i = 0; i < sampleSlots.numSamples; ++i) {
      outputData[i] = dataSum[i] / std::max(static_cast<float>(count), 1.0F);
    }
  }

  // Zero out any input channels that are being downmixed (not remapped back)
  std::unordered_map<int, bool> inputMappedBack;
  for (const auto& [inputChannel, _] : mixerMapping) {
    inputMappedBack[inputChannel] = false;
  }
  for (const auto& [_, outputChannel] : mixerMapping) {
    inputMappedBack[outputChannel] = true;  // mark this channel as final target
  }

  for (const auto& [inputChannel, _] : mixerMapping) {
    if (!inputMappedBack[inputChannel]) {
      std::fill(sampleSlots.sampleSlots[inputChannel].begin(),
                sampleSlots.sampleSlots[inputChannel].begin() +
                    sampleSlots.numSamples,
                0.0F);
    }
  }
}