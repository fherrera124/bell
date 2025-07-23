#include "bell/audio/Codec.h"

// Standard includes
#include <vector>

// Own includes
#include "bell/audio/Types.h"

// Library includes
#include "aacdecoder_lib.h"
#include "aacenc_lib.h"

namespace bell::audio {
class AACCodec : public Codec {
 public:
  AACCodec() = default;
  ~AACCodec() override;

  enum class AACMode {
    AAC_LC,  // Low Complexity

    // TODO: Support for other AAC types
    // Currently only AAC_LC is supported, due to its patent being expired
    HE_AAC,     // High Efficiency AAC
    HE_AAC_V2,  // High Efficiency AAC v2
    AAC_LD,     // Low Delay
    AAC_ELD,    // Enhanced Low Delay
    BSAC,       // Bit-Sliced Arithmetic Coding
    USAC,       // Unified Speech and Audio Coding
  };

  // AAC specific codec configuration
  struct AACCodecConfig : public CodecConfig {
    int transportType = 0;  // Transport type, e.g., TT_MP4_RAW
    std::optional<AACMode> mode =
        AACMode::AAC_LC;  // AAC mode, e.g., AAC_LC, HE_AAC

    std::optional<int> bitrateMode =
        3;  // Optional mode for the VBR bitrate, 3 being medium quality

    std::optional<size_t>
        bitrate;  // Optional CBR bitrate in bits per second, overrides bitrateMode when set

    std::optional<std::vector<uint8_t>>
        decoderAudioSpecificConfig;  // Optional decoder audio specific config
  };

  // Delete copy constructor and copy assignment operator
  AACCodec(const AACCodec&) = delete;
  AACCodec& operator=(const AACCodec&) = delete;

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
  const char* LOG_TAG = "audio::AACCodec";

  AACENCODER* encoder = nullptr;
  AAC_DECODER_INSTANCE* decoder = nullptr;
  CStreamInfo* streamInfo = nullptr;  // Stream info for decoded audio

  // tmp buffer for encode / decode calls
  std::vector<uint8_t> tmpBuffer;

  // Codec configuration
  AACCodecConfig config{};
};
}  // namespace bell::audio

namespace bell {
using AACCodec = audio::AACCodec;
using AACCodecConfig = audio::AACCodec::AACCodecConfig;
}  // namespace bell
