#include "bell/audio/Codec.h"

// Standard includes
#include <vector>

#include "bell/audio/Common.h"
#include "ivorbiscodec.h"

namespace bell::audio {

class VorbisCodec : public Codec {
 public:
  VorbisCodec() = default;
  ~VorbisCodec() override;

  // Vorbis specific codec configuration
  struct VorbisCodecConfig {};

  // Delete copy constructor and copy assignment operator
  VorbisCodec(const VorbisCodec&) = delete;
  VorbisCodec& operator=(const VorbisCodec&) = delete;

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
  const char* LOG_TAG = "audio::VorbisCodec";

  // Codec configuration
  VorbisCodecConfig config{};

  vorbis_info vorbisInfo;
  vorbis_comment vorbisComment;
  vorbis_dsp_state* vorbisDspState;
};
}  // namespace bell::audio

namespace bell {
using VorbisCodec = audio::VorbisCodec;
using VorbisCodecConfig = audio::VorbisCodec::VorbisCodecConfig;
}  // namespace bell
