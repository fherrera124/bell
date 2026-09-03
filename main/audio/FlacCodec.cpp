#include "bell/audio/FlacCodec.h"

// Standard includes
#include <algorithm>
#include <cassert>
#include <cstring>

// Own includes
#include "bell/Logger.h"
#include "bell/audio/Codec.h"
#include "bell/audio/Common.h"
#include "FLAC/stream_decoder.h"
#include "nonstd/expected.hpp"

using namespace bell::audio;

FlacCodec::~FlacCodec() {
  if (decoder) {
    FLAC__stream_decoder_delete(decoder);
  }
}

bell::Result<> FlacCodec::setupEncode(const AudioFormat& audioFormat,
                                      const CodecConfig& codecSpecificConfig) {
  (void)audioFormat;
  (void)codecSpecificConfig;
  // libFLAC is vendored decode-only here
  return nonstd::make_unexpected(audio::Errc::OperationNotSupported);
}

bell::Result<Codec::EncodeResult> FlacCodec::encode(
    tcb::span<const std::byte> pcmInput) {
  (void)pcmInput;
  return nonstd::make_unexpected(audio::Errc::OperationNotSupported);
}

bell::Result<> FlacCodec::setupDecode(const AudioFormat& audioFormat,
                                      const CodecConfig& codecSpecificConfig) {
  if (!std::holds_alternative<FlacConfig>(codecSpecificConfig)) {
    return nonstd::make_unexpected(Errc::UnsupportedConfig);
  }
  config = std::get<FlacConfig>(codecSpecificConfig);

  // Re-setup (mid-stream codec change) - start from a fresh decoder rather
  // than trying to reuse the old one's state.
  if (decoder) {
    FLAC__stream_decoder_delete(decoder);
  }
  decoder = FLAC__stream_decoder_new();
  if (!decoder) {
    BELL_LOG(error, LOG_TAG, "Failed to allocate flac decoder");
    return nonstd::make_unexpected(audio::Errc::CodecError);
  }

  pcmScratch.clear();
  pcmScratch.reserve(config.bufferSize / sizeof(int16_t));
  this->audioFormat = audioFormat;
  streamInfoSeen = false;
  hadError = false;

  auto initStatus = FLAC__stream_decoder_init_stream(
      decoder, &FlacCodec::readCallback, nullptr, nullptr, nullptr, nullptr,
      &FlacCodec::writeCallback, &FlacCodec::metadataCallback,
      &FlacCodec::errorCallback, this);

  if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    BELL_LOG(error, LOG_TAG, "Failed to init flac decoder: {}",
             FLAC__StreamDecoderInitStatusString[initStatus]);
    return nonstd::make_unexpected(audio::Errc::CodecError);
  }

  return {};
}

bell::Result<Codec::SetupStatus> FlacCodec::setupDecodeFromHeaders(
    tcb::span<const std::byte> encodedInput) {
  assert(decoder != nullptr);

  pendingInput = encodedInput;
  inputCursor = 0;
  hadError = false;

  FLAC__stream_decoder_process_until_end_of_metadata(decoder);

  if (streamInfoSeen) {
    return SetupStatus::Ready;
  }
  if (hadError) {
    BELL_LOG(info, LOG_TAG, "Failed to process flac metadata, err = {}",
             FLAC__StreamDecoderErrorStatusString[lastError]);
    return nonstd::make_unexpected(make_flac_error_code(lastError));
  }
  return SetupStatus::Incomplete;
}

bell::Result<Codec::DecodeResult> FlacCodec::decode(
    tcb::span<const std::byte> encodedInput) {
  assert(decoder != nullptr);

  // process_until_end_of_stream, not process_single in a loop - the
  // bit-reader buffers ahead, so a byte-count-based loop can stop with
  // frames still buffered undecoded. Flush first: this leaves the decoder
  // parked in END_OF_STREAM, making every later call a no-op otherwise.
  FLAC__stream_decoder_flush(decoder);

  pendingInput = encodedInput;
  inputCursor = 0;
  hadError = false;
  pcmScratch.clear();

  if (!FLAC__stream_decoder_process_until_end_of_stream(decoder)) {
    BELL_LOG(info, LOG_TAG, "Could not decode flac chunk, state = {}",
             FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(
                 decoder)]);
    return nonstd::make_unexpected(
        hadError ? make_flac_error_code(lastError)
                 : make_error_code(audio::Errc::CodecError));
  }

  if (pcmScratch.empty()) {
    return nonstd::make_unexpected(audio::Errc::NotEnoughBytes);
  }

  return DecodeResult{
      .pcm = {reinterpret_cast<std::byte*>(pcmScratch.data()),
              pcmScratch.size() * sizeof(int16_t)},
      .consumedInputBytes = encodedInput.size(),
  };
}

void FlacCodec::resetDecoderState() {
  if (decoder) {
    FLAC__stream_decoder_flush(decoder);
  }
}

FLAC__StreamDecoderReadStatus FlacCodec::readCallback(
    const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes,
    void* clientData) {
  (void)decoder;
  auto* self = static_cast<FlacCodec*>(clientData);

  size_t available = self->pendingInput.size() - self->inputCursor;
  if (available == 0) {
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }

  size_t toCopy = std::min(*bytes, available);
  std::memcpy(buffer, self->pendingInput.data() + self->inputCursor, toCopy);
  self->inputCursor += toCopy;
  *bytes = toCopy;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus FlacCodec::writeCallback(
    const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* clientData) {
  (void)decoder;
  auto* self = static_cast<FlacCodec*>(clientData);

  uint32_t channels = frame->header.channels;
  uint32_t blocksize = frame->header.blocksize;
  uint32_t bitsPerSample = frame->header.bits_per_sample;

  // bell's Codec output is always interleaved S16 - downscale wider source
  // samples (libFLAC decodes to the stream's native bit depth) to fit.
  int shift = bitsPerSample > 16 ? static_cast<int>(bitsPerSample - 16) : 0;

  size_t base = self->pcmScratch.size();
  self->pcmScratch.resize(base + static_cast<size_t>(blocksize) * channels);

  for (uint32_t ch = 0; ch < channels; ch++) {
    const FLAC__int32* src = buffer[ch];
    int16_t* dest = self->pcmScratch.data() + base + ch;
    for (uint32_t i = 0; i < blocksize; i++) {
      dest[i * channels] = static_cast<int16_t>(src[i] >> shift);
    }
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacCodec::metadataCallback(const FLAC__StreamDecoder* decoder,
                                 const FLAC__StreamMetadata* metadata,
                                 void* clientData) {
  (void)decoder;
  auto* self = static_cast<FlacCodec*>(clientData);

  if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
    return;
  }

  const auto& streamInfo = metadata->data.stream_info;
  self->audioFormat =
      audio::Format(static_cast<int>(streamInfo.channels), SampleFormat::S16,
                    static_cast<SampleRate>(streamInfo.sample_rate));
  self->maxBlockSize = streamInfo.max_blocksize;
  self->streamInfoSeen = true;
}

void FlacCodec::errorCallback(const FLAC__StreamDecoder* decoder,
                              FLAC__StreamDecoderErrorStatus status,
                              void* clientData) {
  (void)decoder;
  auto* self = static_cast<FlacCodec*>(clientData);
  self->lastError = status;
  self->hadError = true;
  BELL_LOG(info, self->LOG_TAG, "flac decode error: {}",
           FLAC__StreamDecoderErrorStatusString[status]);
}
