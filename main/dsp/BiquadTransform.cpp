#include "bell/dsp/BiquadTransform.h"

// Standard includes
#include <mutex>

// IQmathLib
#include <IQmathLib.h>

using namespace bell::dsp;

void BiquadTransform::addStage(const BiquadParameters& params) {
  std::scoped_lock lock(accessMutex);

  // Add the stage to the list
  stages.push_back({
      .params = params,
  });

  recalculateCoefficients();
}

void BiquadTransform::sampleRateUpdated(const audio::SampleRate sampleRate) {
  std::scoped_lock lock(accessMutex);

  if (this->sampleRate != sampleRate) {
    this->sampleRate = sampleRate;

    // Coefficients are dependent on the sample rate
    recalculateCoefficients();
  }
}

void BiquadTransform::recalculateCoefficients() {
  for (auto& stage : stages) {
    auto coeffs = stage.params.calculateCoefficients(this->sampleRate);

    // Assign the coefficients to the stage
    stage.a1 = coeffs[0];
    stage.a2 = coeffs[1];
    stage.b0 = coeffs[2];
    stage.b1 = coeffs[3];
    stage.b2 = coeffs[4];

    // Reset the state variables
    stage.x1 = 0;
    stage.x2 = 0;
    stage.y1 = 0;
    stage.y2 = 0;
    stage.savedFractional = 0;
  }
}

float BiquadTransform::calculateHeadroom() {
  return 0.0F;
}

void BiquadTransform::process(DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  const size_t numStages = stages.size();
  if (numStages == 0) {
    return;  // No stages to process
  }

  int32_t* data = (*sampleSlots.primarySlot)[this->channels[0]];
  if (data == nullptr) {
    return;  // Invalid channel
  }

  const size_t numSamples = sampleSlots.numSamples;

  // Direct form 1 biquad filter with basic noise shaping
  // Process each stage sequentially (stage-major order is faster for IIR filters)
  // Based on Robert Bristow-Johnson code
  for (size_t stageIdx = 0; stageIdx < numStages; ++stageIdx) {
    auto& stage = stages[stageIdx];

    // Pre-load state into local variables for better register allocation
    int32_t x1 = stage.x1;
    int32_t x2 = stage.x2;
    int32_t y1 = stage.y1;
    int32_t y2 = stage.y2;

    // Pre-load coefficients into local variables
    const int32_t b0 = stage.b0;
    const int32_t b1 = stage.b1;
    const int32_t b2 = stage.b2;
    const int32_t a1 = stage.a1;
    const int32_t a2 = stage.a2;

    int64_t acc = stage.savedFractional;

    for (size_t i = 0; i < numSamples; i++) {
      int32_t x0 = data[i];

      // IQ30 * IQ28 = IQ58
      acc += (int64_t)b0 * x0;
      acc += (int64_t)b1 * x1;
      acc += (int64_t)b2 * x2;
      acc += (int64_t)a1 * y1;
      acc += (int64_t)a2 * y2;

      // Saturation to prevent wrapping
      if (acc > 0x07FFFFFFFFFFFFFFLL) {
        acc = 0x07FFFFFFFFFFFFFFLL;
      } else if (acc < -0x0800000000000000LL) {
        acc = -0x0800000000000000LL;
      }

      // Quantization: IQ58 -> IQ30
      int32_t y0 = static_cast<int32_t>(acc >> 28);

      x2 = x1;
      x1 = x0;
      y2 = y1;
      y1 = y0;

      // Keep fractional bits for noise shaping
      acc &= 0x0FFFFFFFLL;

      // Write output
      data[i] = y0;
    }

    // Store state back to memory
    stage.x1 = x1;
    stage.x2 = x2;
    stage.y1 = y1;
    stage.y2 = y2;
    stage.savedFractional = static_cast<int32_t>(acc);
  }
}
