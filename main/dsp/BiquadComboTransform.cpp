#include "bell/dsp/BiquadComboTransform.h"

// Standar includes
#include <cmath>
#include <mutex>
#include <optional>

// Bell includes
#include "bell/dsp/BiquadTransform.h"

using namespace bell::dsp;

void BiquadComboTransform::configure(Type filterType, float freq, int order) {
  std::scoped_lock lock(accessMutex);
  biquads.clear();

  switch (filterType) {
    case Type::LR_Lowpass:
      configureLinkwitzRiley(freq, order, true);
      break;
    case Type::LR_Highpass:
      configureLinkwitzRiley(freq, order, false);
      break;
    case Type::BW_Lowpass:
      configureButterworth(freq, order, true);
      break;
    case Type::BW_Highpass:
      configureButterworth(freq, order, false);
      break;
  }
}

std::vector<float> BiquadComboTransform::calculateBWQ(int order) {
  std::vector<float> qValues;
  for (int n = 0; n < order / 2; n++) {
    float q = 1.0F / (2.0F * sinf(M_PI / order * n + 0.5F));
    qValues.push_back(q);
  }

  if (order % 2 > 0) {
    qValues.push_back(-1.0);
  }

  return qValues;
}

std::vector<float> BiquadComboTransform::calculateLRQ(int order) {
  auto qValues = calculateBWQ(order / 2);

  if (order % 4 > 0) {
    qValues.pop_back();
    qValues.insert(qValues.end(), qValues.begin(), qValues.end());
    qValues.push_back(0.5F);
  } else {
    qValues.insert(qValues.end(), qValues.begin(), qValues.end());
  }

  return qValues;
}

void BiquadComboTransform::configureButterworth(float freq, int order,
                                                bool isLowpass) {
  std::vector<float> qValues = calculateBWQ(order);
  for (const auto& q : qValues) {
    auto filter = std::make_unique<BiquadTransform>();
    filter->setChannels(channels);

    if (q >= 0.0) {
      filter->configure(isLowpass ? BiquadTransform::Type::Lowpass
                                  : BiquadTransform::Type::Highpass,
                        freq, q, std::nullopt, std::nullopt, std::nullopt);
    } else {
      filter->configure(isLowpass ? BiquadTransform::Type::LowpassFO
                                  : BiquadTransform::Type::HighpassFO,
                        freq, std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt);
    }

    this->biquads.push_back(std::move(filter));
  }
}

void BiquadComboTransform::configureLinkwitzRiley(float freq, int order,
                                                  bool isLowpass) {
  std::vector<float> qValues = calculateLRQ(order);
  for (const auto& q : qValues) {
    auto filter = std::make_unique<BiquadTransform>();
    filter->setChannels(channels);

    if (q >= 0.0) {
      filter->configure(isLowpass ? BiquadTransform::Type::Lowpass
                                  : BiquadTransform::Type::Highpass,
                        freq, q, std::nullopt, std::nullopt, std::nullopt);
    } else {
      filter->configure(isLowpass ? BiquadTransform::Type::LowpassFO
                                  : BiquadTransform::Type::HighpassFO,
                        freq, std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt);
    }

    this->biquads.push_back(std::move(filter));
  }
}

void BiquadComboTransform::process(DataSlots& sampleSlots) {
  std::scoped_lock lock(accessMutex);

  for (auto& biquad : biquads) {
    biquad->process(sampleSlots);
  }
}

float BiquadComboTransform::calculateHeadroom() {
  float headroom = 0.0F;
  for (const auto& biquad : biquads) {
    headroom += biquad->calculateHeadroom();
  }

  return headroom;
}