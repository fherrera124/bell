#include "bell/dsp/MixerTransform.h"

#include "IQmathLib.h"

using namespace bell;

void dsp::MixerTransform::configure(
    const std::vector<std::vector<int>>& mixerMapping) {
  std::scoped_lock lock(accessMutex);
  this->mixerMapping = {};
  for (const auto& channel : mixerMapping) {
    this->mixerMapping.push_back({});
    for (const auto& output : channel) {
      this->mixerMapping.back()[output] = true;
    }
  }
}

float dsp::MixerTransform::calculateHeadroom() {
  return 0.0F;  // No headroom required for mixer
}

void dsp::MixerTransform::process(dsp::DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  for (size_t channelIdx = 0; channelIdx < this->mixerMapping.size(); channelIdx++) {
    

  }
}