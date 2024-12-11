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

  virtual uint8_t* encode(const uint8_t* pcmInput, size_t inputLength,
                          size_t& outputLength, ResultCode& result) = 0;
  virtual uint8_t* decode(const uint8_t* pcmInput, size_t inputLength,
                          size_t& outputLength, ResultCode& result) = 0;

 protected:
  /// Params assigned by the implementation
  bell::audio::Format audioFormat{};
  std::optional<uint16_t> frameLength{};
};
}  // namespace bell::codec