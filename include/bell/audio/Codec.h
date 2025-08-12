#pragma once

// Standard includes
#include <any>
#include <cstddef>
#include <optional>

// Common headers, required before bell result
#include "bell/Result.h"
#include "bell/audio/Common.h"

namespace bell::audio {
/**
 * Base class for audio codecs
 */
class Codec {
 public:
  Codec() = default;
  virtual ~Codec() = default;

  /**
   * @brief Setups the codec in encode mode
   *
   * @param audioFormat PCM audio format to use with this codec
   * @param samplesPerFrame optional, preferred samples-per-frame value
   * @param codecSpecificConfig codec-specific configuration, see implementation details
   * @return Result of the operation, might return Errc::UnsupportedConfig if codec does not support the requested config
   */
  virtual bell::Result<> setupEncode(
      const AudioFormat& audioFormat,
      std::optional<int> samplesPerFrame = std::nullopt,
      const std::any& codecSpecificConfig = {}) = 0;

  /**
   * @brief Setups the codec in decode mode
   *
   * @param audioFormat PCM audio format to use with this codec
   * @param samplesPerFrame optional, preferred samples-per-frame value
   * @param codecSpecificConfig codec-specific configuration, see implementation details
   * @return Result of the operation, might return Errc::UnsupportedConfig if codec does not support the requested config
   */
  virtual bell::Result<> setupDecode(
      const AudioFormat& audioFormat,
      std::optional<int> samplesPerFrame = std::nullopt,
      const std::any& codecSpecificConfig = {}) = 0;

  /**
   * @brief Returns the full codec configuration
   */
  virtual std::any getConfig() const = 0;

  /**
   * @brief Returns the configured bitwidth
   */
  virtual bell::audio::Format getAudioFormat() const = 0;

  /**
   * @brief Encodes the input PCM data, returning the encoded data
   *
   * @param pcmInput Pointer to the input PCM data, in the format specified by the audioFormat
   * @param inputLength Length of the input PCM data
   * @param outputLength Pointer to size_t to store the length of the encoded data
   * @return uint8_t* Pointer to the encoded data, or error
   */
  virtual bell::Result<std::byte*> encode(const std::byte* pcmInput,
                                          size_t inputLength,
                                          size_t& outputLength) = 0;

  /**
   * @brief Decodes the input encoded data, returning the decoded PCM data
   *
   * @param encodedInput Pointer to the input encoded data
   * @param inputLength Length of the input encoded data
   * @param outputLength Pointer to size_t to store the length of the decoded PCM data
   * @param result Result code of the operation, indicating success, error, or need for more data
   * @return std::byte* Pointer to the decoded PCM data, or error
   */
  virtual bell::Result<std::byte*> decode(const std::byte* encodedInput,
                                          size_t inputLength,
                                          size_t& outputLength) = 0;

 protected:
  AudioFormat audioFormat{};
  std::optional<int> samplesPerFrame{};
};
}  // namespace bell::audio

namespace bell {
using AudioCodec = audio::Codec;
}
