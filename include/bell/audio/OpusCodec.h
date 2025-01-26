#include "bell/audio/Codec.h"

// Standard includes
#include <vector>

// Own includes
#include "bell/audio/Types.h"

// fwd declare
class OpusEncoder;
class OpusDecoder;

namespace bell::audio {
class OpusCodec : public Codec {
 public:
  OpusCodec() = default;
  ~OpusCodec() override;

  // Opus specific codec configuration
  struct OpusCodecConfig : public CodecConfig {
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
  void setupEncode(const std::any& codecConfig) override;
  void setupDecode(const std::any& codecConfig) override;
  uint8_t* encode(const uint8_t* pcmInput, size_t inputLength,
                  size_t& outputLength, ResultCode& result) override;
  uint8_t* decode(const uint8_t* encodedInput, size_t inputLength,
                  size_t& outputLength, ResultCode& result) override;
  std::any getConfig() const override { return config; }
  audio::Format getAudioFormat() const override { return config.audioFormat; }

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
