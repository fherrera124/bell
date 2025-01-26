#pragma once

// Only include this file if the Portaudio backend is enabled
#include "bell/audio/Types.h"
#ifdef BELL_BACKEND_PORTAUDIO

// Standard includes
#include <mutex>

// bell includes
#include "bell/audio/OutputBackend.h"

namespace bell::audio {
/**
 * @brief Audio output backend using the Portaudio library
 */
class PortAudioOutput : public OutputBackend {
 public:
  PortAudioOutput() = default;
  ~PortAudioOutput() override;

  struct PortAudioOutputConfig : public OutputBackendConfig {
    // Portaudio specific configuration
    int deviceIndex = -1;  // Device index to use, -1 for default
    int framesPerBuffer =
        1024;  // Frames per buffer, 0 for paFramesPerBufferUnspecified
    double suggestedLatency = 0.000;  // Suggested latency in seconds
  };

  // Delete copy constructor and copy assignment operator
  PortAudioOutput(const PortAudioOutput&) = delete;
  PortAudioOutput& operator=(const PortAudioOutput&) = delete;

  // Output implementation
  void configure(const std::any& outputConfig) override;
  uint32_t write(const uint8_t* pcmData, size_t length) override;

 private:
  const char* LOG_TAG = "PortAudioBackend";
  // Portaudio stream
  void* stream = nullptr;
  std::mutex configMutex;

  // Flag to check if Portaudio is initialized
  bool portaudioInitialized = false;

  // Configured audio format
  audio::Format audioFormat;
};
}  // namespace bell::audio

namespace bell {
// Type alias for the Portaudio audio output backend
using PortAudioOutput = audio::PortAudioOutput;
using PortAudioOutputConfig = audio::PortAudioOutput::PortAudioOutputConfig;
};  // namespace bell

#endif