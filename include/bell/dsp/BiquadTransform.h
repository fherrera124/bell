#pragma once

// Standard includes
#include <array>
#include <optional>

// bell includes
#include "TransformPipeline.h"

namespace bell::dsp {
class BiquadTransform : public Transform {
 public:
  BiquadTransform() = default;

  enum class Type {
    Free,
    Highpass,
    Lowpass,
    HighpassFO,
    LowpassFO,
    Peaking,
    Highshelf,
    HighshelfFO,
    Lowshelf,
    LowshelfFO,
    Notch,
    Bandpass,
    Allpass,
    AllpassFO
  };

  static Type stringToType(const std::string& type) {
    const std::unordered_map<std::string, Type> typeMap = {
        {"free", Type::Free},
        {"highpass", Type::Highpass},
        {"lowpass", Type::Lowpass},
        {"highpass_fo", Type::HighpassFO},
        {"lowpass_fo", Type::LowpassFO},
        {"peaking", Type::Peaking},
        {"highshelf", Type::Highshelf},
        {"highshelf_fo", Type::HighshelfFO},
        {"lowshelf", Type::Lowshelf},
        {"lowshelf_fo", Type::LowshelfFO},
        {"notch", Type::Notch},
        {"bandpass", Type::Bandpass},
        {"allpass", Type::Allpass},
        {"allpass_fo", Type::AllpassFO},
    };

    if (typeMap.find(type) == typeMap.end()) {
      throw std::invalid_argument("Invalid filter type");
    }

    return typeMap.at(type);
  }

  Type getFilterType() const { return filterType; }

  // Set the filter type, frequency, Q, gain, slope and bandwidth. Not all parameters are used for all filter types.
  void configure(Type filterType, std::optional<float> f,
                 std::optional<float> q, std::optional<float> gain,
                 std::optional<float> slope, std::optional<float> bandwidth);

  void configureFree(float a1, float a2, float b0, float b1, float b2);

  // Transform implementation, see Transform.h for details
  void process(DataSlots& sampleSlots) override;
  float calculateHeadroom() override;

 private:
  const char* LOG_TAG = "BiquadTransform";
  // IQ30 format
  std::array<int32_t, 5> coeffs{};
  int64_t accumulator = 0;
  int32_t x1 = 0;
  int32_t x2 = 0;
  int32_t y1 = 0;
  int32_t y2 = 0;

  // Pi constant
  const float FLOAT_PI = M_PI;

  // Will be set by the first call to process
  float sampleRate = 44100.0;

  // Filter type
  Type filterType = Type::Free;

  std::optional<float> fValue;
  std::optional<float> qValue;
  std::optional<float> gainValue;
  std::optional<float> slopeValue;
  std::optional<float> bandwidthValue;

  // Generator methods for different filter types
  void highPassCoEffs(float f, float q);
  void highPassFOCoEffs(float f);
  void lowPassCoEffs(float f, float q);
  void lowPassFOCoEffs(float f);

  void peakCoEffs(float f, float gain, float q);
  void peakCoEffsBandwidth(float f, float gain, float bandwidth);

  void highShelfCoEffs(float f, float gain, float q);
  void highShelfCoEffsSlope(float f, float gain, float slope);
  void highShelfFOCoEffs(float f, float gain);

  void lowShelfCoEffs(float f, float gain, float q);
  void lowShelfCoEffsSlope(float f, float gain, float slope);
  void lowShelfFOCoEffs(float f, float gain);

  void notchCoEffs(float f, float q);
  void notchCoEffsBandwidth(float f, float bandwidth);

  void bandPassCoEffs(float f, float q);
  void bandPassCoEffsBandwidth(float f, float bandwidth);

  void allPassCoEffs(float f, float q);
  void allPassCoEffsBandwidth(float f, float bandwidth);
  void allPassFOCoEffs(float f);

  void normalizeCoEffs(float a0, float a1, float a2, float b0, float b1,
                       float b2);
};
}  // namespace bell::dsp

namespace bell {
using BiquadTransform = dsp::BiquadTransform;
}
