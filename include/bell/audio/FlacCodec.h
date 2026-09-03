#include "bell/audio/Codec.h"

// Standard includes
#include <cstddef>
#include <vector>

// Library includes
#include "bell/audio/Common.h"
#include "FLAC/stream_decoder.h"

namespace bell::audio {
namespace internal {
// Map libFLAC's per-frame decode error status to std::error_code
struct flac_error_category : public std::error_category {
  const char* name() const noexcept override { return "FLAC"; }
  std::string message(int ev) const noexcept override {
    if (ev < 0 || ev >= FLAC__STREAM_DECODER_ERROR_STATUS_MISSING_FRAME + 1) {
      return "Unknown FLAC error";
    }
    return FLAC__StreamDecoderErrorStatusString[ev];
  }
};

inline const flac_error_category flacErrorCategory{};
}  // namespace internal

// std::error_code helper
inline std::error_code make_flac_error_code(FLAC__StreamDecoderErrorStatus err) {
  return {static_cast<int>(err), internal::flacErrorCategory};
};

class FlacCodec : public Codec {
 public:
  FlacCodec() = default;
  ~FlacCodec() override;

  // Delete copy constructor and copy assignment operator
  FlacCodec(const FlacCodec&) = delete;
  FlacCodec& operator=(const FlacCodec&) = delete;

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

  void resetDecoderState() override;

  // STREAMINFO's max block size (samples per channel), 0 before it's been
  // parsed. A constant-blocksize stream's frames all use this size; only
  // its last frame may be shorter.
  uint32_t getMaxBlockSize() const { return maxBlockSize; }

 private:
  const char* LOG_TAG = "audio::FlacCodec";

  FLAC__StreamDecoder* decoder = nullptr;

  // Codec configuration
  FlacConfig config{};

  AudioFormat audioFormat;
  bool streamInfoSeen = false;
  uint32_t maxBlockSize = 0;

  // Input span currently being drained by readCallback, and how far into
  // it the decoder has read so far.
  tcb::span<const std::byte> pendingInput;
  size_t inputCursor = 0;

  // Owned, interleaved S16 output - libFLAC's own per-channel buffers are
  // reused/overwritten on the next frame, so decode() can't return a span
  // into them directly.
  std::vector<int16_t> pcmScratch;

  FLAC__StreamDecoderErrorStatus lastError =
      FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC;
  bool hadError = false;

  static FLAC__StreamDecoderReadStatus readCallback(
      const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes,
      void* clientData);
  static FLAC__StreamDecoderWriteStatus writeCallback(
      const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame,
      const FLAC__int32* const buffer[], void* clientData);
  static void metadataCallback(const FLAC__StreamDecoder* decoder,
                               const FLAC__StreamMetadata* metadata,
                               void* clientData);
  static void errorCallback(const FLAC__StreamDecoder* decoder,
                            FLAC__StreamDecoderErrorStatus status,
                            void* clientData);
};
}  // namespace bell::audio

namespace bell {
using FlacCodec = audio::FlacCodec;
}  // namespace bell
