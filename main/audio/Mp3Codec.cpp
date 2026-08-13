#include "bell/audio/Mp3Codec.h"

// Standard includes
#include <cstddef>

// Own includes
#include "bell/Logger.h"
#include "bell/audio/Codec.h"
#include "bell/audio/Common.h"
#include "mp3dec.h"

using namespace bell::audio;

Mp3Codec::~Mp3Codec() {
  closeDecoder();
}

void Mp3Codec::openDecoder() {
  decoder = MP3InitDecoder();
  pcmScratch.resize(MAX_NGRAN * MAX_NSAMP * MAX_NCHAN);
}

void Mp3Codec::closeDecoder() {
  if (decoder) {
    MP3FreeDecoder(decoder);
    decoder = nullptr;
  }
}

bell::Result<> Mp3Codec::setupEncode(const AudioFormat& audioFormat,
                                     const CodecConfig& codecSpecificConfig) {
  (void)audioFormat;
  (void)codecSpecificConfig;
  // libhelix-mp3 is decode-only
  return nonstd::make_unexpected(Errc::OperationNotSupported);
}

bell::Result<> Mp3Codec::setupDecode(const AudioFormat& audioFormat,
                                     const CodecConfig& codecSpecificConfig) {
  if (!std::holds_alternative<Mp3Config>(codecSpecificConfig)) {
    return nonstd::make_unexpected(Errc::UnsupportedConfig);
  }

  closeDecoder();
  openDecoder();
  this->audioFormat = audioFormat;

  return {};
}

bell::Result<Codec::SetupStatus> Mp3Codec::setupDecodeFromHeaders(
    tcb::span<const std::byte> encodedInput) {
  if (headerParsed) {
    return SetupStatus::Ready;
  }

  if (!decoder) {
    openDecoder();
  }

  auto* buf = reinterpret_cast<unsigned char*>(
      const_cast<std::byte*>(encodedInput.data()));

  MP3FrameInfo frameInfo{};
  int res = MP3GetNextFrameInfo(decoder, &frameInfo, buf);
  if (res != ERR_MP3_NONE) {
    BELL_LOG(error, LOG_TAG, "Failed to parse MP3 frame header: {}", res);
    return nonstd::make_unexpected(make_helix_mp3_error_code(res));
  }

  // decode() always hands out stereo (see its own comment) - matched
  // here so getAudioFormat() is consistent before decode() has run too.
  audioFormat = audio::Format{2, SampleFormat::S16,
                              static_cast<SampleRate>(frameInfo.samprate)};
  headerParsed = true;
  return SetupStatus::Ready;
}

bell::Result<Codec::EncodeResult> Mp3Codec::encode(
    tcb::span<const std::byte> pcmInput) {
  (void)pcmInput;
  return nonstd::make_unexpected(Errc::OperationNotSupported);
}

bell::Result<Codec::DecodeResult> Mp3Codec::decode(
    tcb::span<const std::byte> encodedInput) {
  if (!decoder) {
    return nonstd::make_unexpected(Errc::CodecError);
  }

  auto* inPtr = reinterpret_cast<unsigned char*>(
      const_cast<std::byte*>(encodedInput.data()));
  int bytesLeft = static_cast<int>(encodedInput.size());

  int res = MP3Decode(decoder, &inPtr, &bytesLeft, pcmScratch.data(), 0);
  size_t consumed = encodedInput.size() - static_cast<size_t>(bytesLeft);

  if (res != ERR_MP3_NONE) {
    if (res == ERR_MP3_MAINDATA_UNDERFLOW || res == ERR_MP3_INDATA_UNDERFLOW) {
      // Frame bytes are consumed either way - reservoir still filling is
      // success with empty PCM, not an error.
      return DecodeResult{.pcm = {}, .consumedInputBytes = consumed};
    }
    BELL_LOG(error, LOG_TAG, "Failed to decode MP3 frame: {}", res);
    return nonstd::make_unexpected(make_helix_mp3_error_code(res));
  }

  MP3FrameInfo frameInfo{};
  MP3GetLastFrameInfo(decoder, &frameInfo);
  audioFormat = audio::Format{2, SampleFormat::S16,
                              static_cast<SampleRate>(frameInfo.samprate)};

  size_t outputSamps = static_cast<size_t>(frameInfo.outputSamps);
  if (frameInfo.nChans == 1) {
    // AudioSinkI2S is fixed at stereo - duplicate mono samples to L+R.
    // Backward in-place: each write (2i, 2i+1) is always >= i, so it
    // never clobbers a source sample before it's read.
    for (size_t i = outputSamps; i-- > 0;) {
      pcmScratch[2 * i] = pcmScratch[i];
      pcmScratch[2 * i + 1] = pcmScratch[i];
    }
    outputSamps *= 2;
  }

  return DecodeResult{
      .pcm = {reinterpret_cast<std::byte*>(pcmScratch.data()),
              outputSamps * sizeof(int16_t)},
      .consumedInputBytes = consumed,
  };
}

void Mp3Codec::resetDecoderState() {
  if (decoder) {
    closeDecoder();
    openDecoder();
  }
}
