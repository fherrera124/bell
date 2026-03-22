#pragma once

#include "bell/audio/Codec.h"

// Standard includes
#include <memory_resource>

// Include HQLC
extern "C" {
#include "hqlc.h"
}

namespace bell::audio {
namespace internal {
// Map HQLC error codes to std::error_code
struct hqlc_error_category : public std::error_category {
  const char* name() const noexcept override { return "HQLC"; }
  std::string message(int ev) const noexcept override {
    switch (static_cast<hqlc_error>(ev)) {
      case HQLC_OK:
        return "No error occurred.";
      case HQLC_ERR_INVALID_ARG:
        return "NULL pointer or out-of-range parameter.";
      case HQLC_ERR_UNSUPPORTED_RATE:
        return "Unsupported sample rate (must be 48000).";
      case HQLC_ERR_UNSUPPORTED_CHANNELS:
        return "Unsupported channel count (must be 1 or 2).";
      case HQLC_ERR_BUFFER_TOO_SMALL:
        return "Output buffer capacity insufficient.";
      case HQLC_ERR_BITSTREAM_CORRUPT:
        return "Decoder encountered invalid bitstream.";
      default:
        return "Unknown HQLC error";
    }
  }
};

const hqlc_error_category hqlcErrorCategory{};
}  // namespace internal

// std::error_code helper
inline std::error_code make_hqlc_error_code(int err) {
  return {static_cast<int>(err), internal::hqlcErrorCategory};
};

class HQLCCodec : public Codec {
 public:
  HQLCCodec() = default;
  ~HQLCCodec() override;

  // Delete copy constructor and copy assignment operator
  HQLCCodec(const HQLCCodec&) = delete;
  HQLCCodec& operator=(const HQLCCodec&) = delete;

  // Codec implementation
  bell::Result<> setupEncode(const AudioFormat& audioFormat,
                             const CodecConfig& codecSpecificConfig) override;
  bell::Result<> setupDecode(const AudioFormat& audioFormat,
                             const CodecConfig& codecSpecificConfig) override;
  bell::Result<SetupStatus> setupDecodeFromHeaders(
      tcb::span<const std::byte> encodedInput) override {
    (void)encodedInput;
    return nonstd::make_unexpected(Errc::OperationNotSupported);
  };
  bell::Result<EncodeResult> encode(
      tcb::span<const std::byte> pcmInput) override;
  bell::Result<DecodeResult> decode(
      tcb::span<const std::byte> encodedInput) override;

  audio::Format getAudioFormat() const override { return audioFormat; }

 private:
  const char* LOG_TAG = "audio::HQLCCodec";

  AudioFormat audioFormat;

  // Codec configuration
  HQLCConfig config{};

  hqlc_pcm_format pcmFormat = HQLC_PCM16;

  // Memory resource for allocations
  std::pmr::memory_resource* memoryResource = nullptr;

  // Encoder/Decoder handles
  hqlc_encoder* encoder = nullptr;
  size_t encoderSize = 0;
  hqlc_decoder* decoder = nullptr;
  size_t decoderSize = 0;

  // Scratch buffers
  uint8_t* encoderScratch = nullptr;
  size_t encoderScratchSize = 0;
  uint8_t* decoderScratch = nullptr;
  size_t decoderScratchSize = 0;

  // Output buffers for encode/decode
  std::byte* encodeOutputBuffer = nullptr;
  size_t encodeOutputBufferSize = 0;
  std::byte* decodeOutputBuffer = nullptr;
  size_t decodeOutputBufferSize = 0;
};
}  // namespace bell::audio

namespace bell {
using HQLCCodec = audio::HQLCCodec;
using HQLCConfig = audio::HQLCConfig;
}  // namespace bell
