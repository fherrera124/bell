#pragma once

// Standard includes
#include <utility>
#include <vector>

// Bell includes
#include "TransformPipeline.h"

namespace bell::dsp {
/**
 * @brief MixerTransform allows for either downmixing or upmixing of audio channels.
 */
class MixerTransform : public Transform {
 public:
  MixerTransform() = default;

  /**
   * @brief Configure the mixer with the given map of input channel to output channels
   *
   * For example, to downmix stereo (0, 1) to mono (0), use [ {0, 0}, {1, 0} ]
   * To upmix mono (0) to stereo (0, 1), use [ {0, 0}, {0, 1} ]
   *
   * @param mixerMap Vector of pairs of input channel to output channel
   */
  void configure(const std::vector<std::pair<int, int>>& mixerMapping);

  // Transform implementation, see Transform.h for details
  void process(DataSlots& sampleSlots) override;
  float calculateHeadroom() override;

 private:
  // Mixer config
  std::vector<std::pair<int, int>> mixerMapping;

  // A map to keep track of how many source channels are contributing to each target channel
  std::unordered_map<int, std::pair<ChannelData, int>> outputDataAcc{};

  // Calculates the input and output size
  // Currently unused
  int sourceChannels = 0;
  int targetChannels = 0;
};
}  // namespace bell::dsp

namespace bell {
using MixerTransform = dsp::MixerTransform;
}
