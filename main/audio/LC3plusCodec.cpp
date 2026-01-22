#include "bell/audio/LC3plusCodec.h"

// Standard includes
#include <cassert>
#include <cstring>

// Own includes
#include "bell/Logger.h"
#include "nonstd/expected.hpp"

using namespace bell::audio;

LC3plusCodec::~LC3plusCodec() {
  encoder = nullptr;
  decoder = nullptr;

  // Free allocated memory using the memory resource
  if (memoryResource) {
    if (decoderMem) {
      memoryResource->deallocate(decoderMem, decoderMemSize,
                                 alignof(std::max_align_t));
      decoderMem = nullptr;
      decoderMemSize = 0;
    }
    if (decodeTmpBuffer) {
      memoryResource->deallocate(decodeTmpBuffer, decodeTmpBufferSize,
                                 alignof(std::max_align_t));
      decodeTmpBuffer = nullptr;
      decodeTmpBufferSize = 0;
    }
    if (inputBuffer) {
      memoryResource->deallocate(inputBuffer, inputBufferSize,
                                 alignof(std::max_align_t));
      inputBuffer = nullptr;
      inputBufferSize = 0;
    }
  }
}

bell::Result<> LC3plusCodec::setupEncode(
    const AudioFormat& audioFormat, const CodecConfig& codecSpecificConfig) {
  // Check if the config is of the correct type
  if (!std::holds_alternative<LC3plusConfig>(codecSpecificConfig)) {
    return nonstd::make_unexpected(Errc::UnsupportedConfig);
  }
  config = std::get<LC3plusConfig>(codecSpecificConfig);

  // Set up memory resource
  memoryResource = config.memoryResource ? config.memoryResource
                                         : std::pmr::new_delete_resource();

  // Validate sample rate - LC3 HR mode supports 48000 and 96000
  auto sampleRate = audioFormat.getSampleRateValue();
  bool hrmode = (sampleRate == 96000);

  if (!LC3_HR_CHECK_SR_HZ(hrmode, sampleRate)) {
    BELL_LOG(warn, LOG_TAG,
             "Unsupported sample rate: {}. LC3 supports 8000, 16000, 24000, "
             "32000, 48000, and 96000 Hz",
             sampleRate);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  this->audioFormat = audioFormat;
  int channels = audioFormat.getNumChannels();

  // Determine frame duration
  uint32_t samplesPerPacket = config.samplesPerPacket.value_or(480);
  dtUs = audioFormat.samplesToMs(samplesPerPacket) * 1000;

  if (!LC3_CHECK_DT_US(dtUs)) {
    BELL_LOG(error, LOG_TAG, "Invalid frame duration: {} us", dtUs);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Get frame samples
  frameSamples = lc3_hr_frame_samples(hrmode, dtUs, sampleRate);
  if (frameSamples < 0) {
    BELL_LOG(error, LOG_TAG, "Failed to get frame samples");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Calculate target frame size in bytes
  int bitrate = config.bitrate.value_or((sampleRate >= 48000) ? 128000 : 96000);
  nbytes = lc3_hr_frame_bytes(hrmode, dtUs, sampleRate, bitrate);
  if (nbytes < 0) {
    BELL_LOG(error, LOG_TAG, "Failed to calculate frame bytes for bitrate {}",
             bitrate);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  BELL_LOG(info, LOG_TAG,
           "LC3 encoder setup: {}Hz, {} channels, {}us frame, {} bytes/frame, "
           "{} samples/frame",
           sampleRate, channels, dtUs, nbytes, frameSamples);

  // Allocate encoder memory
  unsigned encoderSize = lc3_hr_encoder_size(hrmode, dtUs, sampleRate);
  if (encoderSize == 0) {
    BELL_LOG(error, LOG_TAG, "Failed to get encoder size");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  encoderMem.resize(encoderSize);

  // Setup encoder (sr_pcm_hz = 0 means no resampling)
  encoder =
      lc3_hr_setup_encoder(hrmode, dtUs, sampleRate, 0, encoderMem.data());
  if (!encoder) {
    BELL_LOG(error, LOG_TAG, "Failed to setup LC3 encoder");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Allocate output buffer (per channel)
  tmpBuffer.resize(nbytes * channels);

  return {};
}

bell::Result<> LC3plusCodec::setupDecode(
    const AudioFormat& audioFormat, const CodecConfig& codecSpecificConfig) {
  // Check if the config is of the correct type
  if (!std::holds_alternative<LC3plusConfig>(codecSpecificConfig)) {
    return nonstd::make_unexpected(Errc::UnsupportedConfig);
  }
  config = std::get<LC3plusConfig>(codecSpecificConfig);

  // Set up memory resource
  memoryResource = config.memoryResource ? config.memoryResource
                                         : std::pmr::new_delete_resource();

  // Validate sample rate
  auto sampleRate = audioFormat.getSampleRateValue();
  bool hrmode = (sampleRate == 96000);

  if (!LC3_HR_CHECK_SR_HZ(hrmode, sampleRate)) {
    BELL_LOG(warn, LOG_TAG,
             "Unsupported sample rate: {}. LC3 supports 8000, 16000, 24000, "
             "32000, 48000, and 96000 Hz",
             sampleRate);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  this->audioFormat = audioFormat;
  int channels = audioFormat.getNumChannels();

  // Determine frame duration
  uint32_t samplesPerPacket = config.samplesPerPacket.value_or(480);
  dtUs = audioFormat.samplesToMs(samplesPerPacket) * 1000;

  if (!LC3_CHECK_DT_US(dtUs)) {
    BELL_LOG(error, LOG_TAG, "Invalid frame duration: {} us", dtUs);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Get frame samples
  frameSamples = lc3_hr_frame_samples(hrmode, dtUs, sampleRate);
  if (frameSamples < 0) {
    BELL_LOG(error, LOG_TAG, "Failed to get frame samples");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Calculate expected frame size in bytes
  int bitrate = config.bitrate.value_or((sampleRate >= 48000) ? 128000 : 96000);
  nbytes = lc3_hr_frame_bytes(hrmode, dtUs, sampleRate, bitrate);
  if (nbytes < 0) {
    BELL_LOG(error, LOG_TAG, "Failed to calculate frame bytes for bitrate {}",
             bitrate);
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  BELL_LOG(info, LOG_TAG,
           "LC3 decoder setup: {}Hz, {} channels, {}us frame, {} bytes/frame, "
           "{} samples/frame",
           sampleRate, channels, dtUs, nbytes, frameSamples);

  // Allocate decoder memory
  unsigned decoderSize = lc3_hr_decoder_size(hrmode, dtUs, sampleRate);
  if (decoderSize == 0) {
    BELL_LOG(error, LOG_TAG, "Failed to get decoder size");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Free old decoder memory if it exists
  if (decoderMem && memoryResource) {
    memoryResource->deallocate(decoderMem, decoderMemSize,
                               alignof(std::max_align_t));
    decoderMem = nullptr;
  }

  // Allocate decoder memory using the memory resource
  decoderMem = static_cast<uint8_t*>(
      memoryResource->allocate(decoderSize, alignof(std::max_align_t)));
  decoderMemSize = decoderSize;

  if (!decoderMem) {
    BELL_LOG(error, LOG_TAG, "Failed to allocate decoder memory");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Setup decoder (sr_pcm_hz = 0 means no resampling)
  decoder = lc3_hr_setup_decoder(hrmode, dtUs, sampleRate, 0, decoderMem);
  if (!decoder) {
    BELL_LOG(error, LOG_TAG, "Failed to setup LC3 decoder");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Allocate output buffer using the memory resource
  size_t pcmBufferSize =
      static_cast<size_t>(frameSamples) * channels * sizeof(int16_t);

  // Free old buffer if it exists
  if (decodeTmpBuffer && memoryResource) {
    memoryResource->deallocate(decodeTmpBuffer, decodeTmpBufferSize,
                               alignof(std::max_align_t));
    decodeTmpBuffer = nullptr;
  }

  // Allocate decode output buffer
  decodeTmpBuffer = static_cast<std::byte*>(
      memoryResource->allocate(pcmBufferSize, alignof(std::max_align_t)));
  decodeTmpBufferSize = pcmBufferSize;

  if (!decodeTmpBuffer) {
    BELL_LOG(error, LOG_TAG, "Failed to allocate decode output buffer");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Pre-allocate input buffer using the memory resource
  size_t inputBufSize = nbytes * channels;

  // Free old buffer if it exists
  if (inputBuffer && memoryResource) {
    memoryResource->deallocate(inputBuffer, inputBufferSize,
                               alignof(std::max_align_t));
    inputBuffer = nullptr;
  }

  // Allocate input buffer
  inputBuffer = static_cast<std::byte*>(
      memoryResource->allocate(inputBufSize, alignof(std::max_align_t)));
  inputBufferSize = inputBufSize;

  if (!inputBuffer) {
    BELL_LOG(error, LOG_TAG, "Failed to allocate decode input buffer");
    return nonstd::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  return {};
}

bell::Result<Codec::EncodeResult> LC3plusCodec::encode(
    tcb::span<const std::byte> pcmInput) {
  assert(encoder != nullptr);

  int channels = audioFormat.getNumChannels();
  int bytesPerSample = 2;  // 16-bit samples

  // Check if we have enough input data
  size_t expectedBytes = frameSamples * channels * bytesPerSample;
  if (pcmInput.size() < expectedBytes) {
    return nonstd::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  const int16_t* input = reinterpret_cast<const int16_t*>(pcmInput.data());

  // Encode each channel separately
  // liblc3 expects interleaved input with stride parameter
  for (int ch = 0; ch < channels; ch++) {
    int result = lc3_encode(
        encoder, LC3_PCM_FORMAT_S16,
        input + ch,                       // Start at channel offset
        channels,                         // Stride for interleaved data
        nbytes,                           // Target frame size
        tmpBuffer.data() + (ch * nbytes)  // Output buffer per channel
    );

    if (result != 0) {
      BELL_LOG(info, LOG_TAG, "Failed to encode LC3 packet for channel {}: {}",
               ch, result);
      return nonstd::make_unexpected(audio::Errc::CodecError);
    }
  }

  return EncodeResult{
      .packets =
          {
              {.data = {tmpBuffer.data(),
                        static_cast<uint32_t>(nbytes * channels)}},
          },
      .consumedInputBytes = expectedBytes};
}

bell::Result<Codec::DecodeResult> LC3plusCodec::decode(
    tcb::span<const std::byte> encodedInput) {
  assert(decoder != nullptr);
  assert(decodeTmpBuffer != nullptr);
  assert(inputBuffer != nullptr);

  int channels = audioFormat.getNumChannels();

  // Check if we have enough input data (nbytes per channel)
  size_t requiredInputSize = static_cast<size_t>(nbytes * channels);
  if (encodedInput.size() < requiredInputSize) {
    return nonstd::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  int16_t* output = reinterpret_cast<int16_t*>(decodeTmpBuffer);

  // Decode each channel separately
  for (int ch = 0; ch < channels; ch++) {
    const uint8_t* channelInput =
        reinterpret_cast<const uint8_t*>(encodedInput.data() + (ch * nbytes));

    int result = lc3_decode(decoder,
                            channelInput,  // Input bitstream for this channel
                            nbytes,        // Frame size
                            LC3_PCM_FORMAT_S16,
                            output + ch,  // Output with channel offset
                            channels      // Stride for interleaved output
    );

    if (result < 0) {
      BELL_LOG(info, LOG_TAG, "Failed to decode LC3 packet for channel {}: {}",
               ch, result);
      return nonstd::make_unexpected(audio::Errc::CodecError);
    }
  }

  size_t outputBytes = frameSamples * channels * sizeof(int16_t);

  return DecodeResult{
      .pcm = {decodeTmpBuffer, outputBytes},
      .consumedInputBytes = requiredInputSize,
  };
}
