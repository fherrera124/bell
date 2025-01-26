#pragma once

// Standard includes
#include <any>

// bell includes
#include "bell/audio/Types.h"

namespace bell::audio {
/**
 * @brief Base class for audio output backends
 */
class OutputBackend {
 public:
  OutputBackend() = default;
  virtual ~OutputBackend() = default;

  // Delete copy constructor and copy assignment operator
  OutputBackend(const OutputBackend&) = delete;
  OutputBackend& operator=(const OutputBackend&) = delete;

  // Generic codec configuration struct, to be extended by the implementation
  struct OutputBackendConfig {
    bell::audio::Format
        audioFormat{};  // Audio format to use, defaults to 16-bit PCM, 2 channels, 44100 Hz
  };

  // Output implementation
  /**
   * @brief Configures the output backend for the given parameters
   *
   * @remark Implementations should validate the configuration and throw an exception if it is invalid 
   * @param outputConfig prefered output configuration
   */
  virtual void configure(const std::any& outputConfig) = 0;

  /**
   * @brief Writes the given PCM data to the output device
   * 
   * @param pcmData PCM data to write
   * @param length Length of the PCM data
   * @return uint32_t Number of bytes written
   */
  virtual uint32_t write(const uint8_t* pcmData, size_t length) = 0;
};
}  // namespace bell::audio

namespace bell {
// Type aliases for the audio output backend
using OutputBackend = audio::OutputBackend;
using OutputBackendConfig = audio::OutputBackend::OutputBackendConfig;
}  // namespace bell