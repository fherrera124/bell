#include "bell/dsp/BiquadTransform.h"

// System includes
#include <cmath>
#include <mutex>

using namespace bell::dsp;

void BiquadTransform::configure(Type filterType, std::optional<float> f,
                                std::optional<float> q,
                                std::optional<float> gain,
                                std::optional<float> slope,
                                std::optional<float> bandwidth) {
  std::scoped_lock lock(accessMutex);
  this->filterType = filterType;
  this->fValue = f;
  this->qValue = q;
  this->gainValue = gain;
  this->slopeValue = slope;
  this->bandwidthValue = bandwidth;

  // Flag to check if the parameters are correct
  bool correctParams = true;

  switch (filterType) {
    case Type::Free:
      this->configureFree(1.0, 1.0, 1.0, 1.0, 1.0);
      break;
    case Type::Highpass:
      if (!f.has_value() || !q.has_value()) {
        correctParams = false;
        break;
      }

      this->highPassCoEffs(f.value(), q.value());
      break;
    case Type::HighpassFO:
      if (!f.has_value()) {
        correctParams = false;
        break;
      }

      this->highPassFOCoEffs(f.value());
      break;
    case Type::Lowpass:
      if (!f.has_value() || !q.has_value()) {
        correctParams = false;
        break;
      }

      this->lowPassCoEffs(f.value(), q.value());
      break;
    case Type::LowpassFO:
      if (!f.has_value()) {
        correctParams = false;
        break;
      }
      this->lowPassFOCoEffs(f.value());
      break;
    case Type::Highshelf:
      // check if config has slope key
      if (slope.has_value()) {
        if (!f.has_value() || !gain.has_value()) {
          correctParams = false;
          break;
        }
        this->highShelfCoEffsSlope(f.value(), gain.value(), slope.value());
      } else {
        if (!f.has_value() || !gain.has_value() || !q.has_value()) {
          correctParams = false;
          break;
        }
        this->highShelfCoEffs(f.value(), gain.value(), q.value());
      }
      break;
    case Type::HighshelfFO:
      if (!f.has_value() || !gain.has_value()) {
        correctParams = false;
        break;
      }
      this->highShelfFOCoEffs(f.value(), gain.value());
      break;
    case Type::Lowshelf:
      // check if config has slope key
      if (slope.has_value()) {
        if (!f.has_value() || !gain.has_value()) {
          correctParams = false;
          break;
        }
        this->lowShelfCoEffsSlope(f.value(), gain.value(), slope.value());
      } else {
        if (!f.has_value() || !gain.has_value() || !q.has_value()) {
          correctParams = false;
          break;
        }
        this->lowShelfCoEffs(f.value(), gain.value(), q.value());
      }
      break;
    case Type::LowshelfFO:
      if (!f.has_value() || !gain.has_value()) {
        correctParams = false;
        break;
      }
      this->lowShelfFOCoEffs(f.value(), gain.value());
      break;
    case Type::Peaking:
      // check if config has bandwidth key
      if (bandwidth.has_value()) {
        if (!f.has_value() || !gain.has_value()) {
          correctParams = false;
          break;
        }
        this->peakCoEffsBandwidth(f.value(), gain.value(), bandwidth.value());
      } else {
        if (!f.has_value() || !gain.has_value() || !q.has_value()) {
          correctParams = false;
          break;
        }
        this->peakCoEffs(f.value(), gain.value(), q.value());
      }
      break;
    case Type::Notch:
      // check if config has bandwidth key
      if (bandwidth.has_value()) {
        if (!f.has_value()) {
          correctParams = false;
          break;
        }
        this->notchCoEffsBandwidth(f.value(), bandwidth.value());
      } else {
        if (!f.has_value() || !q.has_value()) {
          correctParams = false;
          break;
        }
        this->notchCoEffs(f.value(), q.value());
      }
      break;
    case Type::Bandpass:
      // check if config has bandwidth key
      if (bandwidth.has_value()) {
        if (!f.has_value()) {
          correctParams = false;
          break;
        }
        this->bandPassCoEffsBandwidth(f.value(), bandwidth.value());
      } else {
        if (!f.has_value() || !q.has_value()) {
          correctParams = false;
          break;
        }
        this->bandPassCoEffs(f.value(), q.value());
      }
      break;
    case Type::Allpass:
      // check if config has bandwidth key
      if (bandwidth.has_value()) {
        if (!f.has_value()) {
          correctParams = false;
          break;
        }
        this->allPassCoEffsBandwidth(f.value(), bandwidth.value());
      } else {
        if (!f.has_value() || !q.has_value()) {
          correctParams = false;
          break;
        }
        this->allPassCoEffs(f.value(), q.value());
      }
      break;
    case Type::AllpassFO:
      if (!f.has_value()) {
        correctParams = false;
        break;
      }
      this->allPassFOCoEffs(f.value());
      break;
  }

  if (!correctParams) {
    throw std::invalid_argument("Invalid parameters for filter type");
  }
}

void BiquadTransform::configureFree(float a1, float a2, float b0, float b1,
                                    float b2) {
  std::scoped_lock lock(accessMutex);
  // Assign bare coefficients
  this->coeffs[0] = a1;
  this->coeffs[1] = a2;
  this->coeffs[2] = b0;
  this->coeffs[3] = b1;
  this->coeffs[4] = b2;

  // Set the filter type to free
  this->filterType = Type::Free;
}

float BiquadTransform::calculateHeadroom() {
  // TODO: Implement headroom calculation, based on the gain of the filter
  return 0.0F;
}

// coefficients for a high pass biquad filter
void BiquadTransform::highPassCoEffs(float f, float q) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s / (2 * q);

  float b0 = (1 + c) / 2;
  float b1 = -(1 + c);
  float b2 = b0;
  float a0 = 1 + alpha;
  float a1 = -2 * c;
  float a2 = 1 - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a high pass first order biquad filter
void BiquadTransform::highPassFOCoEffs(float f) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float k = tanf(w0 / 2.0F);

  float alpha = 1.0F + k;

  float b0 = 1.0F / alpha;
  float b1 = -1.0F / alpha;
  float b2 = 0.0F;
  float a0 = 1.0F;
  float a1 = -(1.0F - k) / alpha;
  float a2 = 0.0F;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a low pass biquad filter
void BiquadTransform::lowPassCoEffs(float f, float q) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s / (2 * q);

  float b0 = (1 - c) / 2;
  float b1 = 1 - c;
  float b2 = b0;
  float a0 = 1 + alpha;
  float a1 = -2 * c;
  float a2 = 1 - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a low pass first order biquad filter
void BiquadTransform::lowPassFOCoEffs(float f) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float k = tanf(w0 / 2.0F);

  float alpha = 1.0F + k;

  float b0 = k / alpha;
  float b1 = k / alpha;
  float b2 = 0.0F;
  float a0 = 1.0F;
  float a1 = -(1.0F - k) / alpha;
  float a2 = 0.0F;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

// coefficients for a peak biquad filter
void BiquadTransform::peakCoEffs(float f, float gain, float q) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s / (2 * q);

  float ampl = powf(10.0F, gain / 40.0F);
  float b0 = 1.0F + (alpha * ampl);
  float b1 = -2.0F * c;
  float b2 = 1.0F - (alpha * ampl);
  float a0 = 1 + (alpha / ampl);
  float a1 = -2 * c;
  float a2 = 1 - (alpha / ampl);

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::peakCoEffsBandwidth(float f, float gain,
                                          float bandwidth) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s * sinh(logf(2.0F) / 2.0F * bandwidth * w0 / s);

  float ampl = powf(10.0F, gain / 40.0F);
  float b0 = 1.0F + (alpha * ampl);
  float b1 = -2.0F * c;
  float b2 = 1.0F - (alpha * ampl);
  float a0 = 1 + (alpha / ampl);
  float a1 = -2 * c;
  float a2 = 1 - (alpha / ampl);

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::highShelfCoEffs(float f, float gain, float q) {
  float A = powf(10.0F, gain / 40.0F);
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float beta = s * sqrtf(A) / q;
  float b0 = A * ((A + 1.0F) + (A - 1.0F) * c + beta);
  float b1 = -2.0F * A * ((A - 1.0F) + (A + 1.0F) * c);
  float b2 = A * ((A + 1.0F) + (A - 1.0F) * c - beta);
  float a0 = (A + 1.0F) - (A - 1.0F) * c + beta;
  float a1 = 2.0F * ((A - 1.0F) - (A + 1.0F) * c);
  float a2 = (A + 1.0F) - (A - 1.0F) * c - beta;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::highShelfCoEffsSlope(float f, float gain, float slope) {
  float A = powf(10.0F, gain / 40.0F);
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha =
      s / 2.0F *
      sqrtf((A + 1.0F / A) * (1.0F / (slope / 12.0F) - 1.0F) + 2.0F);
  float beta = 2.0F * sqrtf(A) * alpha;
  float b0 = A * ((A + 1.0F) + (A - 1.0F) * c + beta);
  float b1 = -2.0F * A * ((A - 1.0F) + (A + 1.0F) * c);
  float b2 = A * ((A + 1.0F) + (A - 1.0F) * c - beta);
  float a0 = (A + 1.0F) - (A - 1.0F) * c + beta;
  float a1 = 2.0F * ((A - 1.0F) - (A + 1.0F) * c);
  float a2 = (A + 1.0F) - (A - 1.0F) * c - beta;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::highShelfFOCoEffs(float f, float gain) {
  float A = powf(10.0F, gain / 40.0F);
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float tn = tanf(w0 / 2.0F);

  float b0 = A * tn + powf(A, 2);
  float b1 = A * tn - powf(A, 2);
  float b2 = 0.0F;
  float a0 = A * tn + 1.0F;
  float a1 = A * tn - 1.0F;
  float a2 = 0.0F;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::lowShelfCoEffs(float f, float gain, float q) {
  float A = powf(10.0F, gain / 40.0F);
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float beta = s * sqrtf(A) / q;

  float b0 = A * ((A + 1.0F) - (A - 1.0F) * c + beta);
  float b1 = 2.0F * A * ((A - 1.0F) - (A + 1.0F) * c);
  float b2 = A * ((A + 1.0F) - (A - 1.0F) * c - beta);
  float a0 = (A + 1.0F) + (A - 1.0F) * c + beta;
  float a1 = -2.0F * ((A - 1.0F) + (A + 1.0F) * c);
  float a2 = (A + 1.0F) + (A - 1.0F) * c - beta;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::lowShelfCoEffsSlope(float f, float gain, float slope) {
  float A = powf(10.0F, gain / 40.0F);
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha =
      s / 2.0F *
      sqrtf((A + 1.0F / A) * (1.0F / (slope / 12.0F) - 1.0F) + 2.0F);
  float beta = 2.0F * sqrtf(A) * alpha;

  float b0 = A * ((A + 1.0F) - (A - 1.0F) * c + beta);
  float b1 = 2.0F * A * ((A - 1.0F) - (A + 1.0F) * c);
  float b2 = A * ((A + 1.0F) - (A - 1.0F) * c - beta);
  float a0 = (A + 1.0F) + (A - 1.0F) * c + beta;
  float a1 = -2.0F * ((A - 1.0F) + (A + 1.0F) * c);
  float a2 = (A + 1.0F) + (A - 1.0F) * c - beta;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::lowShelfFOCoEffs(float f, float gain) {
  float A = powf(10.0F, gain / 40.0F);
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float tn = tanf(w0 / 2.0F);

  float b0 = powf(A, 2) * tn + A;
  float b1 = powf(A, 2) * tn - A;
  float b2 = 0.0F;
  float a0 = tn + A;
  float a1 = tn - A;
  float a2 = 0.0F;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::notchCoEffs(float f, float q) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s / (2.0F * q);

  float b0 = 1.0F;
  float b1 = -2.0F * c;
  float b2 = 1.0F;
  float a0 = 1.0F + alpha;
  float a1 = -2.0F * c;
  float a2 = 1.0F - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::notchCoEffsBandwidth(float f, float bandwidth) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s * sinhf(logf(2.0F) / 2.0F * bandwidth * w0 / s);

  float b0 = 1.0F;
  float b1 = -2.0F * c;
  float b2 = 1.0F;
  float a0 = 1.0F + alpha;
  float a1 = -2.0F * c;
  float a2 = 1.0F - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::bandPassCoEffs(float f, float q) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s / (2.0F * q);

  float b0 = alpha;
  float b1 = 0.0F;
  float b2 = -alpha;
  float a0 = 1.0F + alpha;
  float a1 = -2.0F * c;
  float a2 = 1.0F - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::bandPassCoEffsBandwidth(float f, float bandwidth) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s * sinh(logf(2.0F) / 2.0F * bandwidth * w0 / s);

  float b0 = alpha;
  float b1 = 0.0F;
  float b2 = -alpha;
  float a0 = 1.0F + alpha;
  float a1 = -2.0F * c;
  float a2 = 1.0F - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::allPassCoEffs(float f, float q) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s / (2.0F * q);

  float b0 = 1.0F - alpha;
  float b1 = -2.0F * c;
  float b2 = 1.0F + alpha;
  float a0 = 1.0F + alpha;
  float a1 = -2.0F * c;
  float a2 = 1.0F - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::allPassCoEffsBandwidth(float f, float bandwidth) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float c = cosf(w0);
  float s = sinf(w0);
  float alpha = s * sinh(logf(2.0F) / 2.0F * bandwidth * w0 / s);

  float b0 = 1.0F - alpha;
  float b1 = -2.0F * c;
  float b2 = 1.0F + alpha;
  float a0 = 1.0F + alpha;
  float a1 = -2.0F * c;
  float a2 = 1.0F - alpha;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}
void BiquadTransform::allPassFOCoEffs(float f) {
  float w0 = 2 * FLOAT_PI * f / this->sampleRate;
  float tn = tanf(w0 / 2.0F);

  float alpha = (tn + 1.0F) / (tn - 1.0F);

  float b0 = 1.0F;
  float b1 = alpha;
  float b2 = 0.0F;
  float a0 = alpha;
  float a1 = 1.0F;
  float a2 = 0.0F;

  this->normalizeCoEffs(a0, a1, a2, b0, b1, b2);
}

void BiquadTransform::normalizeCoEffs(float a0, float a1, float a2, float b0,
                                      float b1, float b2) {
  coeffs[0] = b0 / a0;
  coeffs[1] = b1 / a0;
  coeffs[2] = b2 / a0;
  coeffs[3] = a1 / a0;
  coeffs[4] = a2 / a0;
}

void BiquadTransform::process(DataSlots& sampleSlots) {
  if (static_cast<float>(sampleSlots.sampleFormat.getSampleRate()) !=
      this->sampleRate) {
    this->sampleRate =
        static_cast<float>(sampleSlots.sampleFormat.getSampleRate());

    // Reconfigure the filter with the new sample rate
    this->configure(this->filterType, this->fValue, this->qValue,
                    this->gainValue, this->slopeValue, this->bandwidthValue);
  }

  auto& input = sampleSlots.sampleSlots.at(this->channels[0]);

#ifdef ESP_PLATFORM
  dsps_biquad_f32_ae32(input.data(), input.data(), sampleSlots.numSamples,
                       coeffs.data(), w.data());
#else
  // Apply the set coefficients
  for (size_t i = 0; i < sampleSlots.numSamples; i++) {
    float d0 = input[i] - coeffs[3] * w[0] - coeffs[4] * w[1];
    input[i] = coeffs[0] * d0 + coeffs[1] * w[0] + coeffs[2] * w[1];
    w[1] = w[0];
    w[0] = d0;
  }
#endif
}
