#pragma once

// System includes
#include <any>
#include <cstddef>
#include <optional>

// Library includes
#include "bell/audio/Types.h"

namespace bell::audio {
/**
 * Base class for audio codecs
 */
class Codec {
 public:
  Codec() = default;
  virtual ~Codec() = default;

  // Generic codec configuration struct, to be extended by the implementation
  struct CodecConfig {
    // Audio format to use, defaults to 16-bit PCM, 2 channels, 44100 Hz
    bell::audio::Format audioFormat{};

    // Optional preferred frame length in milliseconds
    std::optional<uint16_t> frameLength{};
  };

  /// Result code for encode/decode
  enum class ResultCode { Success, NeedsMoreData, Error };

  /**
   * @brief Setups the codec in encode mode
   *
   * @param codecConfig prefered codec configuration
   * @remark after setup, validate if the prefered configuration is applied, as the codec might not support it.
   */
  virtual void setupEncode(const std::any& codecConfig) = 0;

  /**
   * @brief Setups the codec in decode mode
   *
   * @param codecConfig prefered codec configuration
   * @remark after setup, validate if the prefered configuration is applied, as the codec might not support it.
   */
  virtual void setupDecode(const std::any& codecConfig) = 0;

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
   * @param result Result code of the operation, indicating success, error, or need for more data
   * @return uint8_t* Pointer to the encoded data, or nullptr if an error occurred
   */
  virtual uint8_t* encode(const uint8_t* pcmInput, size_t inputLength,
                          size_t& outputLength, ResultCode& result) = 0;

  /**
   * @brief Decodes the input encoded data, returning the decoded PCM data
   *
   * @param encodedInput Pointer to the input encoded data
   * @param inputLength Length of the input encoded data
   * @param outputLength Pointer to size_t to store the length of the decoded PCM data
   * @param result Result code of the operation, indicating success, error, or need for more data
   * @return uint8_t* Pointer to the decoded PCM data, or nullptr if an error occurred
   */
  virtual uint8_t* decode(const uint8_t* encodedInput, size_t inputLength,
                          size_t& outputLength, ResultCode& result) = 0;
};
}  // namespace bell::audio
