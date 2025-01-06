#pragma once

#include <cstdint>

namespace bell::audio {

// Enum class for the bit width of audio samples.
enum class BitWidth : uint8_t {
  BW_8 = 8,
  BW_16 = 16,
  BW_24 = 24,
  BW_32 = 32,
};

// Enum class for the sample rate of audio samples.
enum class SampleRate : uint32_t {
  SR_8000HZ = 8000,
  SR_16000HZ = 16000,
  SR_22050HZ = 22050,
  SR_44100HZ = 44100,
  SR_48000HZ = 48000,
};

// Class for the audio format of audio samples.
class Format {
 public:
  // Default constructor
  Format() : ch(2), bw(BitWidth::BW_16), sr(SampleRate::SR_44100HZ) {}

  Format(uint8_t numChannels, BitWidth bitWidth, SampleRate sampleRate)
      : ch(numChannels), bw(bitWidth), sr(sampleRate) {}

  Format(uint8_t numChannels, uint8_t bitWidth, uint32_t sampleRate)
      : ch(numChannels),
        bw(static_cast<BitWidth>(bitWidth)),
        sr(static_cast<SampleRate>(sampleRate)) {}

  // Getters
  BitWidth getBitWidth() const { return bw; }
  SampleRate getSampleRate() const { return sr; }
  uint8_t getNumChannels() const { return ch; }

  bool operator==(const Format& other) const {
    return bw == other.bw && sr == other.sr && ch == other.ch;
  }

  bool operator!=(const Format& other) const { return !(*this == other); }

  // Convert sample count to bytes
  uint32_t samplesToBytes(uint32_t samples) const {
    return (static_cast<int>(bw) / 8) * samples;
  }

  // Convert bytes to sample count
  uint32_t bytesToSamples(uint32_t bytes) const {
    return bytes / (static_cast<int>(bw) / 8) / ch;
  }

 private:
  uint8_t ch;
  BitWidth bw;
  SampleRate sr;
};
}  // namespace bell::audio