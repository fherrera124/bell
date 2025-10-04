#ifdef BELL_BACKEND_PORTAUDIO

#include "bell/audio/Backend.h"

// Own includes
#include "bell/Logger.h"
#include "bell/Result.h"
#include "bell/audio/Common.h"

// Library includes
#include <portaudio.h>

using namespace bell::audio;

namespace {
// Map bell sample format to PortAudio sample format
bell::Result<PaSampleFormat> getSampleFormat(const SampleFormat& sf) {
  switch (sf) {
    case bell::audio::SampleFormat::S16:
      return paInt16;
    case bell::audio::SampleFormat::S24:
      return paInt24;
    case bell::audio::SampleFormat::S32:
      return paInt32;
    case bell::audio::SampleFormat::F32:
      return paFloat32;
    default:
      return bell::make_unexpected_errc<PaSampleFormat>(
          std::errc::invalid_argument);
  }
}
}  // namespace

// Map PortAudio error codes to std::error_code
struct portaudio_error_category : public std::error_category {
  const char* name() const noexcept override { return "portaudio"; }
  std::string message(int ev) const noexcept override {
    return {Pa_GetErrorText(ev)};
  }
};

// std::error_code helper
inline std::error_code make_portaudio_error_code(int err) {
  return {static_cast<int>(err), portaudio_error_category()};
};

class PortAudioStream : public Backend::Stream {
 public:
  PortAudioStream() = default;

  ~PortAudioStream() override {
    if (streamPtr) {
      PaError err = Pa_CloseStream(streamPtr);
      if (err != paNoError) {
        BELL_LOG(error, LOG_TAG, "Failed to close PortAudio stream: {}",
                 Pa_GetErrorText(err));
      }
    }
  }

  PortAudioStream(const PortAudioStream&) = delete;
  PortAudioStream& operator=(const PortAudioStream&) = delete;

  bell::Result<> initFromParams(const Backend::Device& device,
                                Backend::StreamType streamType,
                                const bell::AudioFormat& format,
                                uint32_t bufferFrames,
                                Backend::AudioCallback callback,
                                const std::any& backendSpecificOptions) {
    (void)backendSpecificOptions;

    this->callback = std::move(callback);
    this->sourceDevice = device;
    this->streamFormat = format;
    this->streamType = streamType;

    auto sampleFormatRes = getSampleFormat(format.getSampleFormat());
    if (!sampleFormatRes) {
      return nonstd::make_unexpected(sampleFormatRes.error());
    }
    PaSampleFormat paFormat = sampleFormatRes.value();

    PaStreamParameters inputParameters{};
    PaStreamParameters outputParameters{};
    PaStreamParameters* inputParamsPtr = nullptr;
    PaStreamParameters* outputParamsPtr = nullptr;

    bool isInput = (streamType == Backend::StreamType::Input) ||
                   (streamType == Backend::StreamType::Duplex);
    bool isOutput = (streamType == Backend::StreamType::Output) ||
                    (streamType == Backend::StreamType::Duplex);

    if (isInput) {
      inputParameters.device = device.id;
      inputParameters.channelCount = format.getNumChannels();
      inputParameters.sampleFormat = paFormat;
      inputParameters.suggestedLatency =
          Pa_GetDeviceInfo(device.id)->defaultLowInputLatency;
      inputParameters.hostApiSpecificStreamInfo = nullptr;
      inputParamsPtr = &inputParameters;
    }

    if (isOutput) {
      outputParameters.device = device.id;
      outputParameters.channelCount = format.getNumChannels();
      outputParameters.sampleFormat = paFormat;
      outputParameters.suggestedLatency =
          Pa_GetDeviceInfo(device.id)->defaultLowOutputLatency;
      outputParameters.hostApiSpecificStreamInfo = nullptr;
      outputParamsPtr = &outputParameters;
    }

    PaError err = Pa_OpenStream(&streamPtr, inputParamsPtr, outputParamsPtr,
                                format.getSampleRateValue(), bufferFrames,
                                paNoFlag, paCallbackShim, this);
    if (err != paNoError) {
      BELL_LOG(error, LOG_TAG, "Failed to open PortAudio stream: {}",
               Pa_GetErrorText(err));
      return nonstd::make_unexpected(make_portaudio_error_code(err));
    }

    return {};
  }

  bell::Result<> start() override {
    if (!streamPtr) {
      return bell::make_unexpected_errc<>(std::errc::bad_file_descriptor);
    }
    PaError err = Pa_StartStream(streamPtr);
    if (err != paNoError) {
      return nonstd::make_unexpected(make_portaudio_error_code(err));
    }
    return {};
  }

  bell::Result<> stop() override {
    if (!streamPtr) {
      return bell::make_unexpected_errc<>(std::errc::bad_file_descriptor);
    }
    PaError err = Pa_StopStream(streamPtr);
    if (err != paNoError) {
      return nonstd::make_unexpected(make_portaudio_error_code(err));
    }
    return {};
  }

  bool isOpen() const override { return streamPtr != nullptr; }

  bool isStarted() const override {
    return streamPtr && Pa_IsStreamActive(streamPtr) == 1;
  }

  bell::AudioFormat getFormat() const override { return streamFormat; }

  float getLatency() const override {
    if (!streamPtr)
      return 0.0f;
    const PaStreamInfo* info = Pa_GetStreamInfo(streamPtr);
    if (!info)
      return 0.0f;
    return static_cast<float>(info->outputLatency *
                              streamFormat.getSampleRateValue());
  }

  Backend::StreamType getType() const override { return streamType; }

 private:
  const char* LOG_TAG = "PortAudioStream";
  PaStream* streamPtr = nullptr;
  Backend::StreamType streamType = Backend::StreamType::Duplex;
  bell::AudioFormat streamFormat;
  Backend::Device sourceDevice;
  Backend::AudioCallback callback;

  static int paCallbackShim(const void* inputBuffer, void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* /*timeInfo*/,
                            PaStreamCallbackFlags /*statusFlags*/,
                            void* userData) {
    auto* self = static_cast<PortAudioStream*>(userData);
    if (self && self->callback) {
      self->callback(reinterpret_cast<uint8_t*>(outputBuffer),
                     reinterpret_cast<const uint8_t*>(inputBuffer),
                     static_cast<uint32_t>(framesPerBuffer));
    }
    return paContinue;
  }
};

class PortAudioBackend : public Backend {
 public:
  PortAudioBackend() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
      throw std::runtime_error("Failed to initialize PortAudio");
    }
  }

  ~PortAudioBackend() override {
    PaError err = Pa_Terminate();
    if (err != paNoError) {
      BELL_LOG(error, LOG_TAG, "Failed to terminate PortAudio: {}",
               Pa_GetErrorText(err));
    }
  }

  PortAudioBackend(const PortAudioBackend&) = delete;
  PortAudioBackend& operator=(const PortAudioBackend&) = delete;

  std::vector<Device> getOutputDevices() override {
    auto allDevices = enumerateDevices();
    allDevices.erase(std::remove_if(allDevices.begin(), allDevices.end(),
                                    [](const Device& device) {
                                      return device.maxOutputChannels == 0;
                                    }),
                     allDevices.end());
    return allDevices;
  }

  std::vector<Device> getInputDevices() override {
    auto allDevices = enumerateDevices();
    allDevices.erase(std::remove_if(allDevices.begin(), allDevices.end(),
                                    [](const Device& device) {
                                      return device.maxInputChannels == 0;
                                    }),
                     allDevices.end());
    return allDevices;
  }

  bell::Result<Device> getDefaultOutputDevice() override {
    for (auto& device : enumerateDevices()) {
      if (device.isDefaultOutput) {
        return device;
      }
    }
    return bell::make_unexpected_errc<Device>(std::errc::no_such_device);
  }

  bell::Result<Device> getDefaultInputDevice() override {
    for (auto& device : enumerateDevices()) {
      if (device.isDefaultInput) {
        return device;
      }
    }
    return bell::make_unexpected_errc<Device>(std::errc::no_such_device);
  }

  bell::Result<std::unique_ptr<Stream>> openStream(
      const Device& device, StreamType streamType,
      const bell::AudioFormat& format, uint32_t bufferFrames,
      AudioCallback callback, const std::any& backendSpecificOptions) override {
    auto stream = std::make_unique<PortAudioStream>();

    auto res = stream->initFromParams(device, streamType, format, bufferFrames,
                                      callback, backendSpecificOptions);
    if (!res) {
      return nonstd::make_unexpected(res.error());
    }
    return stream;
  }

 private:
  const char* LOG_TAG = "PortAudioBackend";

  static std::vector<Device> enumerateDevices() {
    std::vector<Device> devices{};
    int numDevices = Pa_GetDeviceCount();
    int defaultInputDevice = Pa_GetDefaultInputDevice();
    int defaultOutputDevice = Pa_GetDefaultOutputDevice();

    for (int i = 0; i < numDevices; ++i) {
      const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
      if (info) {
        devices.push_back(
            Device{static_cast<uint32_t>(i), info->name,
                   i == defaultInputDevice, i == defaultOutputDevice,
                   info->maxInputChannels, info->maxOutputChannels, info});
      }
    }
    return devices;
  }
};

Backend* bell::audio::getPortaudioBackend() {
  static PortAudioBackend backendInstance;
  return &backendInstance;
}

#endif
