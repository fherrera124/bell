#include "bell/audio/OpusCodec.h"

// Standar includes
#include <any>
#include <cassert>
#include <unordered_map>

// Library includes
#include "bell/audio/Common.h"
#include "opus.h"
#include "opus_defines.h"

// Own includes
#include "bell/Logger.h"
#include "tl/expected.hpp"

using namespace bell::audio;

OpusCodec::~OpusCodec() {
  if (encoder) {
    opus_encoder_destroy(encoder);
  }

  if (decoder) {
    opus_decoder_destroy(decoder);
  }
}

int OpusCodec::getOpusFrameSize(int frameDuration) {
  const std::unordered_map<int, int> durationMapping = {
      {5, OPUS_FRAMESIZE_5_MS},     {10, OPUS_FRAMESIZE_10_MS},
      {20, OPUS_FRAMESIZE_20_MS},   {40, OPUS_FRAMESIZE_40_MS},
      {60, OPUS_FRAMESIZE_60_MS},   {80, OPUS_FRAMESIZE_80_MS},
      {100, OPUS_FRAMESIZE_100_MS}, {120, OPUS_FRAMESIZE_120_MS}};

  if (durationMapping.contains(frameDuration)) {
    return durationMapping.at(frameDuration);
  }

  return OPUS_FRAMESIZE_20_MS;
}

bell::Result<> OpusCodec::setupEncode(const AudioFormat& audioFormat,
                                      std::optional<int> samplesPerFrame,
                                      const std::any& codecSpecificConfig) {
  // Check if the config is of the correct type
  try {
    config = std::any_cast<OpusCodecConfig>(codecSpecificConfig);
  } catch (const std::bad_any_cast& err) {
    return tl::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  tmpBuffer.resize(tmpBufferSize);

  // Destroy the encoder if it exists
  if (encoder) {
    opus_encoder_destroy(encoder);
  }

  if (audioFormat.getSampleRate() != SampleRate::SR_48000HZ) {
    BELL_LOG(warn, LOG_TAG, "Opus only supports 48kHz sample rate");
    return tl::make_unexpected(audio::Errc::UnsupportedConfig);
  }
  this->audioFormat = audioFormat;

  int opusError = 0;

  // Allocate opus enc memory and initialize it
  encoder = opus_encoder_create(
      static_cast<int32_t>(audioFormat.getSampleRateValue()),
      audioFormat.getNumChannels(),
      config.application.value_or(OPUS_APPLICATION_AUDIO), &opusError);
  if (opusError != OPUS_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to create opus encoder: {}",
             opus_strerror(opusError));
    return tl::make_unexpected(make_opus_error_code(opusError));
  }

  if (samplesPerFrame.has_value()) {
    this->samplesPerFrame = samplesPerFrame;
    int frameLength =
        static_cast<int>(audioFormat.samplesToMs(samplesPerFrame.value()));
    auto opusDuration = getOpusFrameSize(frameLength);

    // Fallback on 20ms if unsupported
    if (opusDuration == OPUS_FRAMESIZE_20_MS) {
      this->samplesPerFrame = 960;
    }

    // Encoder settings
    opusError =
        opus_encoder_ctl(encoder, OPUS_SET_EXPERT_FRAME_DURATION(opusDuration));
    if (opusError != OPUS_OK) {
      BELL_LOG(error, LOG_TAG, "Failed to set opus frame duration: {}",
               opus_strerror(opusError));
      return tl::make_unexpected(make_opus_error_code(opusError));
    }
  }

  return {};
}

bell::Result<> OpusCodec::setupDecode(const AudioFormat& audioFormat,
                                      std::optional<int> samplesPerFrame,
                                      const std::any& codecSpecificConfig) {
  try {
    config = std::any_cast<OpusCodecConfig>(codecSpecificConfig);
  } catch (const std::bad_any_cast&) {
    return tl::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Resize the tmpbuffer
  tmpBuffer.resize(config.bufferSize);

  if (decoder) {
    opus_decoder_destroy(decoder);
  }

  if (audioFormat.getSampleRate() != SampleRate::SR_48000HZ) {
    BELL_LOG(warn, LOG_TAG, "Opus only supports 48kHz sample rate");
    return tl::make_unexpected(audio::Errc::UnsupportedConfig);
  }
  this->audioFormat = audioFormat;
  this->samplesPerFrame = samplesPerFrame;

  int opusError = 0;

  // Allocate opus enc memory and initialize it
  decoder = opus_decoder_create(
      static_cast<int32_t>(audioFormat.getSampleRateValue()),
      audioFormat.getNumChannels(), &opusError);

  if (opusError != OPUS_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to create opus decoder: {}",
             opus_strerror(opusError));
    // Opus errors most likely come from unsupported config
    return tl::make_unexpected(make_opus_error_code(opusError));
  }

  return {};
}

bell::Result<std::byte*> OpusCodec::encode(const std::byte* pcmInput,
                                           size_t inputLength,
                                           size_t& outputLength) {
  assert(encoder != nullptr);
  int32_t packetSize = opus_encode(
      encoder, reinterpret_cast<const opus_int16*>(pcmInput),
      static_cast<int>(inputLength) / getAudioFormat().getNumChannels(),
      tmpBuffer.data(), static_cast<int>(tmpBuffer.size()));

  // Handle encoded result
  if (packetSize < 0) {
    BELL_LOG(info, LOG_TAG, "Could not encode opus packet, err = {}",
             opus_strerror(packetSize));
    return tl::make_unexpected(make_opus_error_code(packetSize));
  }

  if (packetSize == 0) {
    return tl::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  outputLength = packetSize;
  return reinterpret_cast<std::byte*>(tmpBuffer.data());
}

bell::Result<std::byte*> OpusCodec::decode(const std::byte* encodedInput,
                                           size_t inputLength,
                                           size_t& outputLength) {
  assert(decoder != nullptr);

  auto pcmLen =
      opus_decode(decoder, reinterpret_cast<const unsigned char*>(encodedInput),
                  static_cast<int32_t>(inputLength),
                  reinterpret_cast<opus_int16*>(tmpBuffer.data()),
                  samplesPerFrame.value_or(960), false);

  // Handle encoded result
  if (pcmLen < 0) {
    BELL_LOG(info, LOG_TAG, "Could not decode opus packet, err = {}",
             opus_strerror(pcmLen));
    return tl::make_unexpected(make_opus_error_code(pcmLen));
  }
  if (pcmLen == 0) {
    return tl::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  outputLength = getAudioFormat().samplesToBytes(pcmLen);
  return reinterpret_cast<std::byte*>(tmpBuffer.data());
}
