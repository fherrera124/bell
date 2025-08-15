#pragma once

#include <cstdint>
#include <system_error>

namespace bell::audio {

/**
 * @brief Error enumereation for various audio operations
 */
enum class Errc {
  Success = 0,
  NotEnoughBytes = 1,
  CodecError = 2,
  UnsupportedConfig = 3,
  InvalidFormat = 4
};

namespace internal {
struct audio_error_category : public std::error_category {
  const char* name() const noexcept override { return "BellHTTP"; }
  std::string message(int ev) const noexcept override {
    switch (static_cast<Errc>(ev)) {
      case Errc::Success:
        return "Success";
      case Errc::NotEnoughBytes:
        return "Not enough bytes for operation";
      case Errc::CodecError:
        return "Unknown error during codec operation";
      case Errc::InvalidFormat:
        return "Invalid audio format";
      case Errc::UnsupportedConfig:
        return "Unsupported config";
      default:
        return "Unknown error";
    }
  }
};
}  // namespace internal

// Plug in the error code category for std::error_code
inline std::error_code make_error_code(const bell::audio::Errc& e) {
  return {static_cast<int>(e), bell::audio::internal::audio_error_category()};
};

// Enum class for the bit width of audio samples.
enum class BitWidth : uint8_t {
  BW_8 = 8,
  BW_16 = 16,
  BW_24 = 24,
  BW_32 = 32,
  BW_64 = 64,  // Not commonly used, but included for completeness
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
  Format() = default;

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
  uint32_t getSampleRateValue() const { return static_cast<uint32_t>(sr); }

  // Setters
  void setBitWidth(BitWidth bitWidth) { bw = bitWidth; }
  void setSampleRate(SampleRate sampleRate) { sr = sampleRate; }
  void setNumChannels(uint8_t numChannels) { ch = numChannels; }

  bool operator==(const Format& other) const {
    return bw == other.bw && sr == other.sr && ch == other.ch;
  }

  bool operator!=(const Format& other) const { return !(*this == other); }

  // Convert sample count to bytes
  uint32_t samplesToBytes(uint32_t samples) const {
    return (static_cast<int>(bw) / 8) * samples * ch;
  }

  // Convert bytes to sample count
  uint32_t bytesToSamples(uint32_t bytes) const {
    return bytes / (static_cast<int>(bw) / 8) / ch;
  }

  // Convert milliseconds to samples
  uint32_t msToSamples(uint32_t ms) const {
    return (static_cast<int>(sr) / 1000) * ms;
  }

  // Convert samples to milliseconds
  uint32_t samplesToMs(uint32_t samples) const {
    return samples / (static_cast<int>(sr) / 1000);
  }

  // Convert milliseconds to bytes
  uint32_t msToBytes(uint32_t ms) const {
    return samplesToBytes(msToSamples(ms));
  }

 private:
  uint8_t ch = 2;
  BitWidth bw = BitWidth::BW_16;
  SampleRate sr = SampleRate::SR_44100HZ;
};

}  // namespace bell::audio

namespace std {
template <>
struct is_error_code_enum<bell::audio::Errc> : true_type {};
}  // namespace std

namespace bell {
using AudioFormat = audio::Format;
using SampleRate = audio::SampleRate;
using BitWidth = audio::BitWidth;
}  // namespace bell
