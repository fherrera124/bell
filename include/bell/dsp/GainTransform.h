#pragma once

#include "TransformPipeline.h"

namespace bell::dsp {
/**
  * @brief Gain transform that multiplies the audio samples by a constant gain factor.
  */
class GainTransform : public Transform {
 public:
  GainTransform() = default;

  /**
   * @brief Set the gain of the transform in dB.
   * 
   * @param gainDb Gain in dB
   */
  void configure(float gainDb);

  // Transform implementation, see Transform.h for details
  void process(DataSlots& sampleSlots) override;
  float calculateHeadroom() override;

 private:
  float gainFactor = 1.0F;
  float gainDb = 0.0F;
};
}  // namespace bell::dsp