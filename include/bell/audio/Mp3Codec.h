#pragma once

// Standard includes
#include <cstdint>
#include <vector>

#include "bell/audio/Codec.h"
#include "bell/audio/Common.h"
#include "mp3dec.h"

namespace bell::audio {

namespace internal {
// Maps the ERR_MP3_* codes libhelix-mp3 can return to std::error_code
struct helix_mp3_error_category : public std::error_category {
  const char* name() const noexcept override { return "Helix-MP3"; }
  std::string message(int ev) const noexcept override {
    switch (ev) {
      case ERR_MP3_INDATA_UNDERFLOW:
        return "Input data underflow";
      case ERR_MP3_MAINDATA_UNDERFLOW:
        return "Main data (bit reservoir) underflow";
      case ERR_MP3_FREE_BITRATE_SYNC:
        return "Free-format bitrate sync error";
      case ERR_MP3_OUT_OF_MEMORY:
        return "Out of memory";
      case ERR_MP3_NULL_POINTER:
        return "Null decoder pointer";
      case ERR_MP3_INVALID_FRAMEHEADER:
        return "Invalid frame header";
      case ERR_MP3_INVALID_SIDEINFO:
        return "Invalid side info";
      case ERR_MP3_INVALID_SCALEFACT:
        return "Invalid scale factors";
      case ERR_MP3_INVALID_HUFFCODES:
        return "Invalid Huffman codes";
      case ERR_MP3_INVALID_DEQUANTIZE:
        return "Invalid dequantize";
      case ERR_MP3_INVALID_IMDCT:
        return "Invalid IMDCT";
      case ERR_MP3_INVALID_SUBBAND:
        return "Invalid subband";
      default:
        return "Unknown Helix MP3 error";
    }
  }
};

inline const helix_mp3_error_category helixMp3ErrorCategory{};
}  // namespace internal

inline std::error_code make_helix_mp3_error_code(int err) {
  return {static_cast<int>(err), internal::helixMp3ErrorCategory};
};

class Mp3Codec : public Codec {
 public:
  Mp3Codec() = default;
  ~Mp3Codec() override;

  Mp3Codec(const Mp3Codec&) = delete;
  Mp3Codec& operator=(const Mp3Codec&) = delete;

  // Codec implementation
  bell::Result<> setupEncode(const AudioFormat& audioFormat,
                             const CodecConfig& codecSpecificConfig) override;
  bell::Result<> setupDecode(const AudioFormat& audioFormat,
                             const CodecConfig& codecSpecificConfig) override;
  bell::Result<SetupStatus> setupDecodeFromHeaders(
      tcb::span<const std::byte> encodedInput) override;
  bell::Result<EncodeResult> encode(
      tcb::span<const std::byte> pcmInput) override;
  bell::Result<DecodeResult> decode(
      tcb::span<const std::byte> encodedInput) override;

  audio::Format getAudioFormat() const override { return audioFormat; }

  // Helix has no soft-reset call - rebuilds the decoder handle instead,
  // dropping its bit reservoir.
  void resetDecoderState() override;

 private:
  const char* LOG_TAG = "audio::Mp3Codec";

  HMP3Decoder decoder = nullptr;
  AudioFormat audioFormat;
  bool headerParsed = false;

  // Owned scratch buffer MP3Decode() writes interleaved S16 PCM into - one
  // frame's worth is bounded by MAX_NGRAN*MAX_NSAMP*MAX_NCHAN regardless of
  // the stream's actual rate/channel count.
  std::vector<int16_t> pcmScratch;

  void openDecoder();
  void closeDecoder();
};
}  // namespace bell::audio

namespace bell {
using Mp3Codec = audio::Mp3Codec;
}  // namespace bell
