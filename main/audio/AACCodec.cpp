#include "bell/audio/AACCodec.h"

// Standar includes
#include <cassert>
#include <cstddef>
#include <unordered_map>

// Library includes
#include "FDK_audio.h"
#include "aacdecoder_lib.h"

// Own includes
#include "aacenc_lib.h"
#include "bell/Logger.h"

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
                                            int sampleRate, int channelCount) {
  // Get sampling frequency index
  auto it = samplingFrequencyIndexMap.find(sampleRate);
  if (it == samplingFrequencyIndexMap.end()) {
    throw std::invalid_argument("Unsupported sampling frequency");
  }
  uint8_t sfIndex = it->second;

  uint8_t byte1 = (audioObjectType << 3) | (sfIndex >> 1);
  uint8_t byte2 = ((sfIndex & 1) << 7) | (channelCount << 3);

  return {byte1, byte2};
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

void AACCodec::setupEncode(const std::any& codecConfig) {
  // Check if the config is of the correct type
  config = std::any_cast<AACCodecConfig>(codecConfig);

  if (config.mode != AACMode::AAC_LC) {
    throw std::runtime_error("Unsupported AAC mode for encoding");
  }

  auto res = aacEncOpen(&encoder, 0, config.audioFormat.getNumChannels());
  if (res != AACENC_OK) {
    throw std::runtime_error("Failed to open AAC encoder");
  }

  res = aacEncoder_SetParam(encoder, AACENC_AOT, AOT_MP2_AAC_LC);
  if (res == AACENC_OK) {
    res = aacEncoder_SetParam(
        encoder, AACENC_SAMPLERATE,
        static_cast<int>(config.audioFormat.getSampleRateValue()));
  }
  if (res == AACENC_OK) {
    res = aacEncoder_SetParam(encoder, AACENC_CHANNELMODE,
                              config.audioFormat.getNumChannels());
  }
  if (res == AACENC_OK && config.bitrate.has_value()) {
    // bitrate cbr mode
    res = aacEncoder_SetParam(encoder, AACENC_BITRATE, config.bitrate.value());
  } else if (res == AACENC_OK) {
    // vbr mode
    res =
        aacEncoder_SetParam(encoder, AACENC_BITRATEMODE,
                            config.bitrateMode.value_or(3));  // Default bitrate
  }

  if (res == AACENC_OK) {
    res = aacEncoder_SetParam(encoder, AACENC_TRANSMUX, config.transportType);
  }

  // Ensure default frame length is set
  config.samplesPerFrame = config.samplesPerFrame.value_or(1024);

  if (res == AACENC_OK) {
    res = aacEncoder_SetParam(encoder, AACENC_GRANULE_LENGTH,
                              config.samplesPerFrame.value_or(1024));
  }

  if (res != AACENC_OK) {
    aacEncClose(&encoder);
    encoder = nullptr;
    throw std::runtime_error("Failed to configure AAC encoder");
  }

  // Finalize the encoder configuration
  res = aacEncEncode(encoder, nullptr, nullptr, nullptr, nullptr);
  if (res != AACENC_OK) {
    aacEncClose(&encoder);
    BELL_LOG(error, LOG_TAG,
             "Failed to finalize AAC encoder configuration with error: {}",
             static_cast<int>(res));

    throw std::runtime_error("Failed to initialize AAC encoder");
  }

  // Get encoder info to determine buffer sizes
  AACENC_InfoStruct info;
  if (aacEncInfo(encoder, &info) != AACENC_OK) {
    aacEncClose(&encoder);
    throw std::runtime_error("Failed to get encoder info");
  }
  // Allocate temporary buffer for encoding
  tmpBuffer.resize(info.maxOutBufBytes);

  BELL_LOG(info, LOG_TAG, "AAC Encoder max output buffer size: {}",
           info.maxOutBufBytes);
}

void AACCodec::setupDecode(const std::any& codecConfig) {
  config = std::any_cast<AACCodecConfig>(codecConfig);

  // Ensure default frame length is set
  config.samplesPerFrame = config.samplesPerFrame.value_or(1024);

  if (config.mode != AACMode::AAC_LC) {
    throw std::runtime_error("Unsupported AAC mode for decoding");
  }

  std::vector<uint8_t> asc;

  if (config.decoderAudioSpecificConfig.has_value()) {
    asc = config.decoderAudioSpecificConfig.value();
  } else {
    // Generate default Audio Specific Config
    asc = getAudioSpecificConfig(
        // 2 for AAC_LC, hardcoded for now as we only support AAC_LC
        2, config.audioFormat.getSampleRateValue(),
        config.audioFormat.getNumChannels());
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
    throw std::runtime_error("Failed to open AAC decoder");
  }

  UCHAR* configData = asc.data();
  UCHAR* configArray[1] = {configData};
  UINT byteCount = static_cast<uint8_t>(asc.size());
  AAC_DECODER_ERROR err =
      aacDecoder_ConfigRaw(decoder, configArray, &byteCount);

  if (err != AAC_DEC_OK) {
    aacDecoder_Close(decoder);
    decoder = nullptr;
    throw std::runtime_error("Failed to configure AAC decoder");
  }

  const size_t expectedOutputSize =
      config.audioFormat.samplesToBytes(config.samplesPerFrame.value());

  if (tmpBuffer.size() < expectedOutputSize) {
    tmpBuffer.resize(expectedOutputSize);
  }
}

uint8_t* AACCodec::encode(const uint8_t* pcmInput, size_t inputLength,
                          size_t& outputLength, ResultCode& result) {
  assert(encoder != nullptr);

  INT iidentify = IN_AUDIO_DATA;
  INT oidentify = OUT_BITSTREAM_DATA;
  INT ibufferElementSize = sizeof(INT_PCM);  // 16bit pcm
  INT ibufferSize = static_cast<INT>(inputLength);
  UCHAR* inputBuffer = const_cast<UCHAR*>(pcmInput);

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

  if (err == AACENC_OK) {
    result = ResultCode::Success;
    outputLength = oargs.numOutBytes;
    return tmpBuffer.data();
  }
  BELL_LOG(error, LOG_TAG, "AAC encoding failed with error code: {}",
           static_cast<int>(err));
  result = ResultCode::Error;
  outputLength = 0;
  return nullptr;
}

uint8_t* AACCodec::decode(const uint8_t* encodedInput, size_t inputLength,
                          size_t& outputLength, ResultCode& result) {
  assert(decoder != nullptr);

  result = ResultCode::Success;

  UINT bytesRead = inputLength;
  UINT validBytes = inputLength;

  // Const cast is required due to fdk-aac API only accepting non-const pointers
  UCHAR* inputPtr = const_cast<UCHAR*>(encodedInput);
  UCHAR* bufferArray[1] = {inputPtr};
  AAC_DECODER_ERROR err =
      aacDecoder_Fill(decoder, bufferArray, &bytesRead, &validBytes);

  if (err != AAC_DEC_OK) {
    result = ResultCode::Error;
    return nullptr;
  }
  err = aacDecoder_DecodeFrame(decoder,
                               reinterpret_cast<short*>(tmpBuffer.data()),
                               tmpBuffer.size() / sizeof(short), 0);

  if (err != AAC_DEC_OK) {
    outputLength = 0;
    if (err == AAC_DEC_NOT_ENOUGH_BITS) {
      result = ResultCode::NeedsMoreData;
    } else {
      result = ResultCode::Error;
    }
    return nullptr;
  }

  if (streamInfo == nullptr) {
    streamInfo = aacDecoder_GetStreamInfo(decoder);
  }

  outputLength = static_cast<unsigned long>(streamInfo->frameSize *
                                            streamInfo->numChannels) *
                 sizeof(short);

  return tmpBuffer.data();
}
