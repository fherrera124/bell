#pragma once

// System includes
#include <cstddef>
#include <optional>

// Library includes
#include "bell/audio/Types.h"

namespace bell::codec {
/**
 * Base class for audio codecs
 */
class Codec {
 public:
  Codec() = default;
  virtual ~Codec() = default;

  /// Result code for encode/decode
  enum class ResultCode { Success, NeedsMoreData, Error };

  /**
   * @brief Setups the codec in encode mode
   * 
   * @param params prefered audio stream parameters
   * @param frameLength optional single-frame length in samples
   * @remark after setup, validate if the prefered configuration is applied, as the codec might not support it.
   */
  virtual ResultCode setupEncode(const bell::audio::Format& audioFormat,
                                 std::optional<uint16_t> frameLength = {}) = 0;

  /**
   * @brief Setups the codec in decode mode
   * 
   * @param params prefered audio stream parameters
   * @remark after setup, validate if the prefered configuration is applied, as the codec might not support it.
   */
  virtual ResultCode setupDecode(const bell::audio::Format& audioFormat,
                                 std::optional<uint16_t> frameLength = {}) = 0;

  /**
   * @brief Returns the configured frame length in milliseconds
   */
  std::optional<uint16_t> getFrameLength() const { return this->frameLength; }

  /**
   * @brief Returns the configured bitwidth
   */
  bell::audio::Format getAudioFormat() const { return this->audioFormat; }

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

 protected:
  /// Params assigned by the implementation
  bell::audio::Format audioFormat{};
  std::optional<uint16_t> frameLength{};
};
}  // namespace bell::codec