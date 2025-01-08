#include "bell/audio/OpusCodec.h"

// System includes
#include <cassert>
#include <unordered_map>

// Library includes
#include "opus.h"
#include "opus_defines.h"

// Own includes
#include "bell/Logger.h"

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

void OpusCodec::setupEncode(const std::any& codecConfig) {
  // Check if the config is of the correct type
  config = std::any_cast<OpusCodecConfig>(codecConfig);

  tmpBuffer.resize(tmpBufferSize);

  // Destroy the encoder if it exists
  if (encoder) {
    opus_encoder_destroy(encoder);
  }

  if (config.audioFormat.getSampleRate() != SampleRate::SR_48000HZ) {
    config.audioFormat.setSampleRate(SampleRate::SR_48000HZ);

    BELL_LOG(warn, LOG_TAG,
             "Opus only supports 48kHz sample rate, falling back to 48kHz");
  }

  int opusError = 0;

  // Allocate opus enc memory and initialize it
  encoder = opus_encoder_create(
      static_cast<int32_t>(config.audioFormat.getSampleRateValue()),
      config.audioFormat.getNumChannels(),
      config.application.value_or(OPUS_APPLICATION_AUDIO), &opusError);
  if (opusError != OPUS_OK) {
    throw std::runtime_error(fmt::format("Failed to create opus encoder: {}",
                                         opus_strerror(opusError)));
  }

  if (config.frameLength.has_value()) {
    auto opusDuration = getOpusFrameSize(config.frameLength.value());

    // Fallback on 20ms if unsupported
    if (opusDuration == OPUS_FRAMESIZE_20_MS) {
      config.frameLength = 20;
    }

    // Encoder settings
    opus_encoder_ctl(encoder, OPUS_SET_EXPERT_FRAME_DURATION(opusDuration));
  }
}

void OpusCodec::setupDecode(const std::any& codecConfig) {
  config = std::any_cast<OpusCodecConfig>(codecConfig);

  // Resize the tmpbuffer
  tmpBuffer.resize(config.bufferSize);

  if (decoder) {
    opus_decoder_destroy(decoder);
  }

  if (config.audioFormat.getSampleRate() != SampleRate::SR_48000HZ) {
    config.audioFormat.setSampleRate(SampleRate::SR_48000HZ);

    BELL_LOG(warn, LOG_TAG,
             "Opus only supports 48kHz sample rate, falling back to 48kHz");
  }

  int opusError = 0;

  // Allocate opus enc memory and initialize it
  decoder = opus_decoder_create(
      static_cast<int32_t>(config.audioFormat.getSampleRateValue()),
      config.audioFormat.getNumChannels(), &opusError);
  assert(opusError == OPUS_OK);

  if (opusError != OPUS_OK) {
    throw std::runtime_error(fmt::format("Failed to create opus encoder: {}",
                                         opus_strerror(opusError)));
  }
}

uint8_t* OpusCodec::encode(const uint8_t* pcmInput, size_t inputLength,
                           size_t& outputLength, ResultCode& result) {
  assert(encoder != nullptr);
  int32_t packetSize = opus_encode(
      encoder, reinterpret_cast<const opus_int16*>(pcmInput),
      static_cast<int>(inputLength) / getAudioFormat().getNumChannels(),
      tmpBuffer.data(), static_cast<int>(tmpBuffer.size()));

  // Handle encoded result
  if (packetSize < 0) {
    result = ResultCode::Error;
    return nullptr;
  }

  if (packetSize == 0) {
    result = ResultCode::NeedsMoreData;
    return nullptr;
  }

  result = ResultCode::Success;
  outputLength = packetSize;
  return tmpBuffer.data();
}

uint8_t* OpusCodec::decode(const uint8_t* encodedInput, size_t inputLength,
                           size_t& outputLength, ResultCode& result) {
  assert(decoder != nullptr);
  const int16_t samplesPerPacket = static_cast<int16_t>(
      getAudioFormat().msToSamples(config.frameLength.value_or(20)));

  auto pcmLen = opus_decode(
      decoder, static_cast<const unsigned char*>(encodedInput),
      static_cast<int32_t>(inputLength),
      reinterpret_cast<opus_int16*>(tmpBuffer.data()), samplesPerPacket, false);

  // Handle encoded result
  if (pcmLen < 0) {
    result = ResultCode::Error;
    return nullptr;
  }
  if (pcmLen == 0) {
    result = ResultCode::NeedsMoreData;
    return nullptr;
  }

  result = ResultCode::Success;
  outputLength = getAudioFormat().samplesToBytes(pcmLen) *
                 static_cast<size_t>(getAudioFormat().getNumChannels());

  return tmpBuffer.data();
}
