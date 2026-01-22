#pragma once

#include "bell/audio/Codec.h"

// Standard includes
#include <memory>
#include <memory_resource>
#include <vector>

// Include LC3
extern "C" {
#include "lc3.h"
}

namespace bell::audio {

class LC3plusCodec : public Codec {
 public:
  LC3plusCodec() = default;
  ~LC3plusCodec() override;

  // Delete copy constructor and copy assignment operator
  LC3plusCodec(const LC3plusCodec&) = delete;
  LC3plusCodec& operator=(const LC3plusCodec&) = delete;

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
  const char* LOG_TAG = "audio::LC3plusCodec";

  // Memory resource for allocations
  std::pmr::memory_resource* memoryResource = nullptr;

  // Encoder/Decoder state
  std::vector<uint8_t> encoderMem;
  uint8_t* decoderMem = nullptr;
  size_t decoderMemSize = 0;

  lc3_encoder_t encoder = nullptr;
  lc3_decoder_t decoder = nullptr;

  AudioFormat audioFormat;

  // Codec configuration
  LC3plusConfig config;

  // Frame parameters
  int dtUs = 10000;      // Frame duration in microseconds
  int nbytes = 0;        // Target frame size in bytes
  int frameSamples = 0;  // Number of samples per frame

  // Temporary buffers
  std::vector<std::byte> tmpBuffer;      // Encoder output buffer
  std::byte* decodeTmpBuffer = nullptr;  // Decoder output buffer
  size_t decodeTmpBufferSize = 0;
  std::byte* inputBuffer = nullptr;  // Decode input copy buffer
  size_t inputBufferSize = 0;
};
}  // namespace bell::audio

namespace bell {
using LC3plusCodec = audio::LC3plusCodec;
using LC3plusConfig = audio::LC3plusConfig;
}  // namespace bell
