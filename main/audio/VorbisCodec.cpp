#include "bell/audio/VorbisCodec.h"

// Standar includes
#include <any>
#include <cassert>
#include <cstddef>
#include <unordered_map>

// Library includes

// Own includes
#include "bell/audio/Common.h"
#include "ivorbiscodec.h"

using namespace bell::audio;

VorbisCodec::~VorbisCodec() {}

bell::Result<> VorbisCodec::setupEncode(const AudioFormat& audioFormat,
                                        std::optional<int> samplesPerFrame,
                                        const std::any& codecSpecificConfig) {
  (void)audioFormat;
  (void)samplesPerFrame;
  (void)codecSpecificConfig;

  return {};
}

bell::Result<> VorbisCodec::setupDecode(const AudioFormat& audioFormat,
                                        std::optional<int> samplesPerFrame,
                                        const std::any& codecSpecificConfig) {
  vorbis_info_init(&vorbisInfo);
  vorbis_comment_init(&vorbisComment);

  (void)audioFormat;
  (void)samplesPerFrame;
  (void)codecSpecificConfig;
  return {};
}

bell::Result<std::byte*> VorbisCodec::encode(const std::byte* pcmInput,
                                             size_t inputLength,
                                             size_t& outputLength) {
  (void)pcmInput;
  (void)inputLength;
  (void)outputLength;
  return {};
}

bell::Result<std::byte*> VorbisCodec::decode(const std::byte* encodedInput,
                                             size_t inputLength,
                                             size_t& outputLength) {

                                                 vorbis_synthesis_headerin()
  (void)encodedInput;
  (void)inputLength;
  (void)outputLength;
  return {};
}
