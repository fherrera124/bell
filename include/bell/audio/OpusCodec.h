#include "bell/audio/Codec.h"

// Standard includes
#include <vector>

// Include OPUS errors
#include "bell/audio/Common.h"
#include "opus_defines.h"

// fwd declare
class OpusEncoder;
class OpusDecoder;

namespace bell::audio {
namespace internal {
// Map Opus error codes to std::error_code
struct opus_error_category : public std::error_category {
  const char* name() const noexcept override { return "Opus"; }
  std::string message(int ev) const noexcept override {
    switch (ev) {
      case OPUS_OK:
        return "No error";
      case OPUS_BAD_ARG:
        return "One or more invalid/out of range arguments";
      case OPUS_INTERNAL_ERROR:
        return "An internal error was detected";
      case OPUS_INVALID_PACKET:
        return "Invalid/unsupported request number";
      case OPUS_INVALID_STATE:
        return "An encoder or decoder structure is invalid or already freed";
      case OPUS_ALLOC_FAIL:
        return "Memory allocation has failed";
      default:
        return "Unknown opus error";
    }
  }
};
}  // namespace internal

// std::error_code helper
inline std::error_code make_opus_error_code(int err) {
  return {static_cast<int>(err), internal::opus_error_category()};
};

class OpusCodec : public Codec {
 public:
  OpusCodec() = default;
  ~OpusCodec() override;

  // Opus specific codec configuration
  struct OpusCodecConfig {
    // Opus application, defaults to OPUS_APPLICATION_AUDIO
    std::optional<int> application;

    // This will be the maximum decoded frame size in bytes, in a single decode call.
    // The default is set to be enough to store 100ms of audio at 48kHz, 2 channels, 16-bit.
    uint32_t bufferSize = (100 * 44800 / 1000) * 2 * 2;
  };

  // Delete copy constructor and copy assignment operator
  OpusCodec(const OpusCodec&) = delete;
  OpusCodec& operator=(const OpusCodec&) = delete;

  // Codec implementation
  bell::Result<> setupEncode(const AudioFormat& audioFormat,
                             std::optional<int> samplesPerFrame,
                             const std::any& codecSpecificConfig) override;
  bell::Result<> setupDecode(const AudioFormat& audioFormat,
                             std::optional<int> samplesPerFrame,
                             const std::any& codecSpecificConfig) override;
  bell::Result<std::byte*> encode(const std::byte* pcmInput, size_t inputLength,
                                  size_t& outputLength) override;
  bell::Result<std::byte*> decode(const std::byte* encodedInput,
                                  size_t inputLength,
                                  size_t& outputLength) override;

  std::any getConfig() const override { return config; }
  audio::Format getAudioFormat() const override { return audioFormat; }

 private:
  const char* LOG_TAG = "audio::OpusCodec";
  OpusEncoder* encoder = nullptr;
  OpusDecoder* decoder = nullptr;

  // Codec configuration
  OpusCodecConfig config{};

  /// Max opus frame size is 1275 as from RFC6716.
  static const int tmpBufferSize = 1024 * 2;

  // tmp buffer for encode / decode calls
  std::vector<uint8_t> tmpBuffer;

  /// maps int to OPUS_FRAMESIZE_*
  static int getOpusFrameSize(int frameDuration);
};
}  // namespace bell::audio

namespace bell {
using OpusCodec = audio::OpusCodec;
using OpusCodecConfig = audio::OpusCodec::OpusCodecConfig;
}  // namespace bell
