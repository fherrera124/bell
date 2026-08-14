#include "bell/audio/HQLCCodec.h"

// Standard includes
#include <cassert>
#include <cstring>

// Own includes
#include "bell/Logger.h"
#include "nonstd/expected.hpp"

using namespace bell::audio;

HQLCCodec::~HQLCCodec() {
  if (!memoryResource) {
    return;
  }

  // Encoder resources
  if (encoder) {
    memoryResource->deallocate(encoder, encoderSize, alignof(std::max_align_t));
    encoder = nullptr;
    encoderSize = 0;
  }
  if (encoderScratch) {
    memoryResource->deallocate(encoderScratch, encoderScratchSize,
                               alignof(std::max_align_t));
    encoderScratch = nullptr;
    encoderScratchSize = 0;
  }
  if (encodeOutputBuffer) {
    memoryResource->deallocate(encodeOutputBuffer, encodeOutputBufferSize,
                               alignof(std::max_align_t));
    encodeOutputBuffer = nullptr;
    encodeOutputBufferSize = 0;
  }

  // Decoder resources
  if (decoder) {
    memoryResource->deallocate(decoder, decoderSize, alignof(std::max_align_t));
    decoder = nullptr;
    decoderSize = 0;
  }
  if (decoderScratch) {
    memoryResource->deallocate(decoderScratch, decoderScratchSize,
                               alignof(std::max_align_t));
    decoderScratch = nullptr;
    decoderScratchSize = 0;
  }
  if (decodeOutputBuffer) {
    memoryResource->deallocate(decodeOutputBuffer, decodeOutputBufferSize,
                               alignof(std::max_align_t));
    decodeOutputBuffer = nullptr;
    decodeOutputBufferSize = 0;
  }
}

bell::Result<> HQLCCodec::setupEncode(const AudioFormat& audioFormat,
                                      const CodecConfig& codecSpecificConfig) {
  // Check if the config is of the correct type
  if (!std::holds_alternative<HQLCConfig>(codecSpecificConfig)) {
    return nonstd::make_unexpected(Errc::UnsupportedConfig);
  }
  config = std::get<HQLCConfig>(codecSpecificConfig);

  // Memory resource is latched on first setup: buffers persist across
  // reconfigures, so they must all come from (and return to) one resource
  auto* newResource = config.memoryResource ? config.memoryResource
                                            : std::pmr::new_delete_resource();
  if (!memoryResource) {
    memoryResource = newResource;
  } else if (memoryResource != newResource) {
    BELL_LOG(warn, LOG_TAG,
             "Ignoring memory resource change on codec reconfigure");
  }

  // HQLC only supports 48kHz
  auto sampleRate = audioFormat.getSampleRateValue();
  if (sampleRate != HQLC_SAMPLE_RATE) {
    BELL_LOG(warn, LOG_TAG,
             "Unsupported sample rate: {}. HQLC only supports {} Hz",
             sampleRate, HQLC_SAMPLE_RATE);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  int channels = audioFormat.getNumChannels();
  if (channels < 1 || channels > HQLC_MAX_CHANNELS) {
    BELL_LOG(warn, LOG_TAG,
             "Unsupported channel count: {}. HQLC supports 1-{} channels",
             channels, HQLC_MAX_CHANNELS);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  this->audioFormat = audioFormat;

  // Determine PCM format from sample format
  auto sf = audioFormat.getSampleFormat();
  if (sf == SampleFormat::S24) {
    pcmFormat = HQLC_PCM24;
  } else {
    // Default to S16 for everything else
    pcmFormat = HQLC_PCM16;
  }

  // Build encoder config
  hqlc_encoder_config encCfg{};
  encCfg.channels = static_cast<uint8_t>(channels);
  encCfg.sample_rate = sampleRate;

  if (config.fixedGain.has_value()) {
    encCfg.mode = HQLC_MODE_FIXED;
    encCfg.gain = config.fixedGain.value();
  } else {
    encCfg.mode = HQLC_MODE_RC;
    encCfg.bitrate = config.bitrate.value_or(196000);
  }

  // State size is a compile-time constant, so a reconfigure reuses the
  // existing allocation and only re-inits in place
  if (!encoder) {
    encoderSize = hqlc_encoder_size();
    encoder = reinterpret_cast<hqlc_encoder*>(
        memoryResource->allocate(encoderSize, alignof(std::max_align_t)));
  }

  hqlc_error err = hqlc_encoder_init(encoder, &encCfg);
  if (err != HQLC_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to initialize HQLC encoder: {}",
             static_cast<int>(err));
    memoryResource->deallocate(encoder, encoderSize, alignof(std::max_align_t));
    encoder = nullptr;
    encoderSize = 0;
    return nonstd::make_unexpected(make_hqlc_error_code(static_cast<int>(err)));
  }

  // Scratch and output sizes are constant too - allocate once
  if (!encoderScratch) {
    encoderScratchSize = hqlc_encoder_scratch_size();
    encoderScratch = static_cast<uint8_t*>(memoryResource->allocate(
        encoderScratchSize, alignof(std::max_align_t)));
  }

  if (!encodeOutputBuffer) {
    encodeOutputBufferSize = HQLC_MAX_FRAME_BYTES;
    encodeOutputBuffer = static_cast<std::byte*>(memoryResource->allocate(
        encodeOutputBufferSize, alignof(std::max_align_t)));
  }

  BELL_LOG(info, LOG_TAG,
           "HQLC encoder setup: {}Hz, {} channels, mode={}, pcmFormat={}, enc "
           "size = {}, scratch size = {}",
           sampleRate, channels, config.fixedGain.has_value() ? "fixed" : "rc",
           pcmFormat == HQLC_PCM16 ? "S16" : "S24", encodeOutputBufferSize,
           encoderScratchSize);

  return {};
}

bell::Result<> HQLCCodec::setupDecode(const AudioFormat& audioFormat,
                                      const CodecConfig& codecSpecificConfig) {
  // Check if the config is of the correct type
  if (!std::holds_alternative<HQLCConfig>(codecSpecificConfig)) {
    return nonstd::make_unexpected(Errc::UnsupportedConfig);
  }
  config = std::get<HQLCConfig>(codecSpecificConfig);

  // Memory resource is latched on first setup: buffers persist across
  // reconfigures, so they must all come from (and return to) one resource
  auto* newResource = config.memoryResource ? config.memoryResource
                                            : std::pmr::new_delete_resource();
  if (!memoryResource) {
    memoryResource = newResource;
  } else if (memoryResource != newResource) {
    BELL_LOG(warn, LOG_TAG,
             "Ignoring memory resource change on codec reconfigure");
  }

  // HQLC only supports 48kHz
  auto sampleRate = audioFormat.getSampleRateValue();
  if (sampleRate != HQLC_SAMPLE_RATE) {
    BELL_LOG(warn, LOG_TAG,
             "Unsupported sample rate: {}. HQLC only supports {} Hz",
             sampleRate, HQLC_SAMPLE_RATE);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  int channels = audioFormat.getNumChannels();
  if (channels < 1 || channels > HQLC_MAX_CHANNELS) {
    BELL_LOG(warn, LOG_TAG,
             "Unsupported channel count: {}. HQLC supports 1-{} channels",
             channels, HQLC_MAX_CHANNELS);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  this->audioFormat = audioFormat;

  // Determine PCM format from sample format
  auto sf = audioFormat.getSampleFormat();
  if (sf == SampleFormat::S24) {
    pcmFormat = HQLC_PCM24;
  } else {
    pcmFormat = HQLC_PCM16;
  }

  // State size is a compile-time constant, so a reconfigure reuses the
  // existing allocation and only re-inits in place
  if (!decoder) {
    decoderSize = hqlc_decoder_size();
    decoder = reinterpret_cast<hqlc_decoder*>(
        memoryResource->allocate(decoderSize, alignof(std::max_align_t)));
  }

  hqlc_error err =
      hqlc_decoder_init(decoder, static_cast<uint8_t>(channels), sampleRate);
  if (err != HQLC_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to initialize HQLC decoder: {}",
             static_cast<int>(err));
    memoryResource->deallocate(decoder, decoderSize, alignof(std::max_align_t));
    decoder = nullptr;
    decoderSize = 0;
    return nonstd::make_unexpected(make_hqlc_error_code(static_cast<int>(err)));
  }

  // Scratch size is constant - allocate once
  if (!decoderScratch) {
    decoderScratchSize = hqlc_decoder_scratch_size();
    decoderScratch = static_cast<uint8_t*>(memoryResource->allocate(
        decoderScratchSize, alignof(std::max_align_t)));
  }

  // Output size depends on channels/pcm format - reallocate only on change
  size_t neededOutputSize =
      hqlc_frame_pcm_bytes(static_cast<uint8_t>(channels), pcmFormat);
  if (decodeOutputBuffer && decodeOutputBufferSize != neededOutputSize) {
    memoryResource->deallocate(decodeOutputBuffer, decodeOutputBufferSize,
                               alignof(std::max_align_t));
    decodeOutputBuffer = nullptr;
    decodeOutputBufferSize = 0;
  }
  if (!decodeOutputBuffer) {
    decodeOutputBufferSize = neededOutputSize;
    decodeOutputBuffer = static_cast<std::byte*>(memoryResource->allocate(
        decodeOutputBufferSize, alignof(std::max_align_t)));
  }

  BELL_LOG(info, LOG_TAG, "HQLC decoder setup: {}Hz, {} channels, pcmFormat={}",
           sampleRate, channels, pcmFormat == HQLC_PCM16 ? "S16" : "S24");

  return {};
}

bell::Result<Codec::EncodeResult> HQLCCodec::encode(
    tcb::span<const std::byte> pcmInput) {
  assert(encoder != nullptr);
  assert(encoderScratch != nullptr);
  assert(encodeOutputBuffer != nullptr);

  int channels = audioFormat.getNumChannels();
  size_t expectedBytes =
      hqlc_frame_pcm_bytes(static_cast<uint8_t>(channels), pcmFormat);

  if (pcmInput.size() < expectedBytes) {
    return nonstd::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  size_t outLen = 0;
  hqlc_error err = hqlc_encode_frame(
      encoder, reinterpret_cast<const uint8_t*>(pcmInput.data()), pcmFormat,
      reinterpret_cast<uint8_t*>(encodeOutputBuffer), encodeOutputBufferSize,
      &outLen, encoderScratch);

  if (err != HQLC_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to encode HQLC frame: {}",
             static_cast<int>(err));
    return nonstd::make_unexpected(make_hqlc_error_code(static_cast<int>(err)));
  }

  return EncodeResult{
      .packets =
          {
              {.data = {encodeOutputBuffer, outLen}},
          },
      .consumedInputBytes = expectedBytes,
  };
}

bell::Result<Codec::DecodeResult> HQLCCodec::decode(
    tcb::span<const std::byte> encodedInput) {
  assert(decoder != nullptr);
  assert(decoderScratch != nullptr);
  assert(decodeOutputBuffer != nullptr);

  if (encodedInput.empty()) {
    return nonstd::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  hqlc_error err = hqlc_decode_frame(
      decoder, reinterpret_cast<const uint8_t*>(encodedInput.data()),
      encodedInput.size(), reinterpret_cast<uint8_t*>(decodeOutputBuffer),
      pcmFormat, decoderScratch);

  if (err != HQLC_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to decode HQLC frame: {}",
             static_cast<int>(err));
    return nonstd::make_unexpected(make_hqlc_error_code(static_cast<int>(err)));
  }

  return DecodeResult{
      .pcm = {decodeOutputBuffer, decodeOutputBufferSize},
      .consumedInputBytes = encodedInput.size(),
  };
}

void HQLCCodec::resetDecoderState() {
  // Should reset MDCT state internally
  if (decoder) {
    hqlc_decoder_reset(decoder);
  }
}
