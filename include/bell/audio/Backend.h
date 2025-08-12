#pragma once

// Standard includes
#include <any>

// Own includes
#include "bell/Result.h"
#include "bell/audio/Common.h"

namespace bell::audio {

/**
 * @brief Base class for audio output backends
 */
class Backend {
 public:
  Backend() = default;
  virtual ~Backend() = default;

  // Represents a single audio backend device
  struct Device {
    uint8_t id;
    std::string name;
    bool isDefault;
  };

  // Represents the direction of the audio stream.
  enum class StreamType {
    Output,
    Input,
    Duplex  // Both input and output
  };

  using AudioCallback = std::function<void(
      void* outputBuffer, const void* inputBuffer, unsigned int frameCount)>;

  // Delete copy constructor and copy assignment operator
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  /**
   * @brief Gets a list of all available audio output devices.
   * @return A vector of AudioDevice objects.
   */
  virtual std::vector<Device> getOutputDevices() const = 0;

  /**
   * @brief Gets a list of all available audio input devices.
   * @return A vector of AudioDevice objects.
   */
  virtual std::vector<Device> getInputDevices() const = 0;

  /**
   * @brief Gets the default output device as determined by the system.
   * @return The default AudioDevice.
   */
  virtual bell::Result<Device> getDefaultOutputDevice() const = 0;

  /**
   * @brief Gets the default output device as determined by the system.
   * @return The default AudioDevice.
   */
  virtual bell::Result<Device> getDefaultInputDevice() const = 0;

  /**
   * @brief Opens an audio stream with the specified parameters.
   *
   * @param outputParams Parameters for the output stream.
   * @param inputParams Parameters for the input stream.
   * @param sampleRate The desired sample rate (e.g., 44100, 48000).
   * @param bufferFrames The desired number of frames for the internal buffer. Affects latency.
   * @param callback The user-defined function to be called for audio processing.
   * @param userData A pointer to user data that will be passed to the callback.
   * @param backendSpecificOptions A type-erased object containing options for a specific backend.
   * The caller is responsible for passing a struct that the target backend understands.
   * @return A std::error_code if opening the stream fails.
   */
  virtual bell::Result<> openStream(
      StreamType streamType, const AudioFormat& format, unsigned int sampleRate,
      uint32_t bufferFrames, AudioCallback callback,
      const std::any& backendSpecificOptions = {}) = 0;

  /**
   * @brief Closes the currently open audio stream.
   */
  virtual void closeStream() = 0;

  /**
   * @brief Starts the audio stream, which begins invoking the callback.
   * @return A std::error_code if starting the stream fails.
   */
  virtual bell::Result<> startStream() = 0;

  /**
   * @brief Stops the audio stream, pausing the callback.
   * @return A std::error_code if stopping the stream fails.
   */
  virtual bell::Result<> stopStream() = 0;

  /**
   * @brief Checks if a stream is currently open.
   */
  virtual bool isStreamOpen() const = 0;

  /**
   * @brief Checks if the stream is currently running (i.e., started and not stopped).
   */
  virtual bool isStreamRunning() const = 0;

  /**
   * @brief Gets the actual sample rate of the open stream.
   * This may differ from the requested rate if the hardware doesn't support it.
   */
  virtual SampleRate getStreamSampleRate() const = 0;

  /**
   * @brief Gets the actual latency of the stream in frames.
   */
  virtual double getStreamLatency() const = 0;
};

Backend* getDefaultAudioBackend();

Backend* getPortaudioBackend();
}  // namespace bell::audio

namespace bell {
// Type aliases for the audio output backend
using AudioBackend = audio::Backend;
using AudioBackendDevice = audio::Backend::Device;
using AudioBackendStreamType = audio::Backend::StreamType;
using AudioBackendCallback = audio::Backend::AudioCallback;
}  // namespace bell
