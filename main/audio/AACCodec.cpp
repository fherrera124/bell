#include "bell/audio/AACCodec.h"

// Standar includes
#include <any>
#include <cassert>
#include <cstddef>
#include <unordered_map>

// Library includes

// Own includes
#include "bell/Logger.h"
#include "bell/audio/Common.h"
#include "tl/expected.hpp"

using namespace bell::audio;

namespace {
// Map of sampling frequency to ASC index
const std::unordered_map<size_t, uint8_t> samplingFrequencyIndexMap = {
    {96000, 0},  {88200, 1}, {64000, 2}, {48000, 3}, {44100, 4},
    {32000, 5},  {24000, 6}, {22050, 7}, {16000, 8}, {12000, 9},
    {11025, 10}, {8000, 11}, {7350, 12},
};

// Builds a basic Audio Specific Config (ASC) for AAC. Not all parameters and extensions are supported here, so optionally raw asc structure can be passed to the aaccodecconfig
std::vector<uint8_t> getAudioSpecificConfig(size_t audioObjectType,
                                            int sampleRate, int channelCount,
                                            int use960FrameLen) {
  auto it = samplingFrequencyIndexMap.find(sampleRate);
  if (it == samplingFrequencyIndexMap.end()) {
    return {};
  }
  uint8_t sfIndex = it->second;

  std::vector<uint8_t> asc(3);

  asc[0] = (audioObjectType << 3) | (sfIndex >> 1);
  asc[1] = ((sfIndex & 1) << 7) | (channelCount << 3) |
           (use960FrameLen ? 0x04 : 0x00);
  asc[2] = 0x00;  // dependsOnCoreCoder = 0, extensionFlag = 0

  return asc;
}
}  // namespace

AACCodec::~AACCodec() {
  if (encoder) {
    aacEncClose(&encoder);
    encoder = nullptr;
  }

  if (decoder) {
    aacDecoder_Close(decoder);
    decoder = nullptr;
  }
}

bell::Result<> AACCodec::setupEncode(const AudioFormat& audioFormat,
                                     std::optional<int> samplesPerFrame,
                                     const std::any& codecSpecificConfig) {
  try {
    // Check if the config is of the correct type
    config = std::any_cast<AACCodecConfig>(codecSpecificConfig);
  } catch (const std::bad_any_cast&) {
    return tl::make_unexpected(Errc::UnsupportedConfig);
  }

  if (config.mode != AACMode::AAC_LC) {
    BELL_LOG(error, LOG_TAG, "AAC Codec only supports AAC_LC");
    return tl::make_unexpected(Errc::UnsupportedConfig);
  }

  if (encoder != nullptr) {
    aacEncClose(&encoder);
  }

  this->audioFormat = audioFormat;

  auto res = aacEncOpen(&encoder, 0, audioFormat.getNumChannels());
  if (res != AACENC_OK) {
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  res = aacEncoder_SetParam(encoder, AACENC_AOT, AOT_AAC_LC);
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG, "aacEncoder_SetParam (AACENC_AOT) error {}",
             static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  res = aacEncoder_SetParam(encoder, AACENC_SAMPLERATE,
                            static_cast<int>(audioFormat.getSampleRateValue()));
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG, "aacEncoder_SetParam (AACENC_SAMPLERATE) error {}",
             static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  res = aacEncoder_SetParam(encoder, AACENC_CHANNELMODE,
                            audioFormat.getNumChannels());
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG,
             "aacEncoder_SetParam (AACENC_CHANNELMODE) error {}",
             static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  if (config.bitrate.has_value()) {
    // bitrate cbr mode
    res = aacEncoder_SetParam(encoder, AACENC_BITRATE, config.bitrate.value());
  } else {
    // vbr mode
    res =
        aacEncoder_SetParam(encoder, AACENC_BITRATEMODE,
                            config.bitrateMode.value_or(3));  // Default bitrate
  }

  if (res != AACENC_OK) {
    BELL_LOG(
        error, LOG_TAG,
        "aacEncoder_SetParam (AACENC_BITRATE / AACENC_BITRATEMODE) error {}",
        static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  res = aacEncoder_SetParam(encoder, AACENC_TRANSMUX, config.transportType);
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG, "aacEncoder_SetParam (AACENC_TRANSMUX) error {}",
             static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  // Ensure default frame length is set
  this->samplesPerFrame = samplesPerFrame.value_or(1024);

  res = aacEncoder_SetParam(encoder, AACENC_GRANULE_LENGTH,
                            this->samplesPerFrame.value_or(1024));
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG,
             "aacEncoder_SetParam (AACENC_GRANULE_LENGTH) error {}",
             static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  // Finalize the encoder configuration
  res = aacEncEncode(encoder, nullptr, nullptr, nullptr, nullptr);
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG,
             "Failed to finalize AAC encoder configuration with error: {}",
             static_cast<int>(res));
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }

  // Get encoder info to determine buffer sizes
  AACENC_InfoStruct info;
  res = aacEncInfo(encoder, &info);
  if (res != AACENC_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to get encoder info");
    return tl::make_unexpected(make_fdk_aacenc_error_code(res));
  }
  // Allocate temporary buffer for encoding
  tmpBuffer.resize(info.maxOutBufBytes);

  return {};
}

bell::Result<> AACCodec::setupDecode(const AudioFormat& audioFormat,
                                     std::optional<int> samplesPerFrame,
                                     const std::any& codecSpecificConfig) {
  try {
    config = std::any_cast<AACCodecConfig>(codecSpecificConfig);
  } catch (const std::bad_any_cast&) {
    return tl::make_unexpected(Errc::UnsupportedConfig);
  }

  // Ensure default frame length is set
  this->samplesPerFrame = samplesPerFrame.value_or(1024);
  this->audioFormat = audioFormat;

  if (config.mode != AACMode::AAC_LC) {
    BELL_LOG(error, LOG_TAG, "AAC Codec only supports AAC_LC");
    return tl::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  std::vector<uint8_t> asc;

  if (config.decoderAudioSpecificConfig.has_value()) {
    asc = config.decoderAudioSpecificConfig.value();
  } else {
    // Generate default Audio Specific Config
    asc = getAudioSpecificConfig(
        // 2 for AAC_LC, hardcoded for now as we only support AAC_LC
        2, static_cast<int>(audioFormat.getSampleRateValue()),
        audioFormat.getNumChannels(), this->samplesPerFrame == 960);
  }

  if (asc.empty()) {
    BELL_LOG(error, LOG_TAG, "Could not prepare ASC, invalid sample rate (?)");
    return tl::make_unexpected(audio::Errc::UnsupportedConfig);
  }

  // Cleanup previous decoder if it exists
  if (decoder) {
    aacDecoder_Close(decoder);
    decoder = nullptr;
  }

  streamInfo = nullptr;

  decoder =
      aacDecoder_Open(static_cast<TRANSPORT_TYPE>(config.transportType), 1);

  if (!decoder) {
    return tl::make_unexpected(
        make_fdk_aacdec_error_code(AAC_DEC_INVALID_HANDLE));
  }

  UCHAR* configData = asc.data();
  std::array<UCHAR*, 1> configArray = {configData};
  UINT byteCount = static_cast<uint8_t>(asc.size());
  AAC_DECODER_ERROR err =
      aacDecoder_ConfigRaw(decoder, configArray.data(), &byteCount);

  if (err != AAC_DEC_OK) {
    BELL_LOG(error, LOG_TAG, "aacDecoder_ConfigRaw failed with error: {}",
             static_cast<int>(err));
    return tl::make_unexpected(make_fdk_aacdec_error_code(err));
  }

  const size_t expectedOutputSize =
      audioFormat.samplesToBytes(this->samplesPerFrame.value());

  if (tmpBuffer.size() < expectedOutputSize) {
    tmpBuffer.resize(expectedOutputSize);
  }
  return {};
}

bell::Result<std::byte*> AACCodec::encode(const std::byte* pcmInput,
                                          size_t inputLength,
                                          size_t& outputLength) {
  assert(encoder != nullptr);

  INT iidentify = IN_AUDIO_DATA;
  INT oidentify = OUT_BITSTREAM_DATA;
  INT ibufferElementSize = sizeof(INT_PCM);  // 16bit pcm
  INT ibufferSize = static_cast<INT>(inputLength);
  const UCHAR* constInputBuf = reinterpret_cast<const UCHAR*>(pcmInput);
  // Have to cast down const, due to libfdk api :/
  UCHAR* inputBuffer = const_cast<UCHAR*>(constInputBuf);  // NOLINT

  AACENC_BufDesc inBuf;
  inBuf.numBufs = 1;
  inBuf.bufs = reinterpret_cast<void**>(&inputBuffer);
  inBuf.bufferIdentifiers = &iidentify;
  inBuf.bufSizes = &ibufferSize;
  inBuf.bufElSizes = &ibufferElementSize;

  AACENC_InArgs iargs;
  iargs.numInSamples = static_cast<INT>(inputLength / sizeof(INT_PCM));
  INT obufferElementSize = 1;
  INT obufferSize = static_cast<INT>(tmpBuffer.size());
  AACENC_BufDesc outBuf;
  outBuf.numBufs = 1;
  UCHAR* outBuffer = tmpBuffer.data();
  outBuf.bufs = reinterpret_cast<void**>(&outBuffer);
  outBuf.bufferIdentifiers = &oidentify;
  outBuf.bufSizes = &obufferSize;
  outBuf.bufElSizes = &obufferElementSize;

  AACENC_OutArgs oargs;

  // Call the encoder
  AACENC_ERROR err = aacEncEncode(encoder, &inBuf, &outBuf, &iargs, &oargs);

  if ((err == AACENC_OK) && (oargs.numOutBytes == 0)) {
    return tl::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  if (err == AACENC_OK) {
    outputLength = oargs.numOutBytes;
    return reinterpret_cast<std::byte*>(tmpBuffer.data());
  }

  BELL_LOG(error, LOG_TAG, "AAC encoding failed with error code: {}",
           static_cast<int>(err));
  outputLength = 0;

  return tl::make_unexpected(make_fdk_aacenc_error_code(err));
}

bell::Result<std::byte*> AACCodec::decode(const std::byte* encodedInput,
                                          size_t inputLength,
                                          size_t& outputLength) {
  assert(decoder != nullptr);

  UINT bytesRead = inputLength;
  UINT validBytes = inputLength;

  const UCHAR* constInputPtr = reinterpret_cast<const UCHAR*>(encodedInput);
  // Const cast is required due to fdk-aac API only accepting non-const pointers
  UCHAR* inputPtr = const_cast<UCHAR*>(constInputPtr);  // NOLINT
  std::array<UCHAR*, 1> bufferArray = {inputPtr};
  AAC_DECODER_ERROR err =
      aacDecoder_Fill(decoder, bufferArray.data(), &bytesRead, &validBytes);

  if (err != AAC_DEC_OK) {
    return tl::make_unexpected(make_fdk_aacdec_error_code(err));
  }

  err = aacDecoder_DecodeFrame(
      decoder, reinterpret_cast<short*>(tmpBuffer.data()),
      static_cast<int>(tmpBuffer.size() / sizeof(short)), 0);

  if (err != AAC_DEC_OK) {
    return tl::make_unexpected(make_fdk_aacdec_error_code(err));
  }

  if (streamInfo == nullptr) {
    streamInfo = aacDecoder_GetStreamInfo(decoder);
  }

  // Calculate output len
  outputLength = static_cast<unsigned long>(streamInfo->frameSize *
                                            streamInfo->numChannels) *
                 sizeof(short);

  return reinterpret_cast<std::byte*>(tmpBuffer.data());
}
