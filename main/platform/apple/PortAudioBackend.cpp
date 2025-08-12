#include "bell/audio/Backend.h"

using namespace bell::audio;

class PortAudioBackend : public Backend {
 public:
  PortAudioBackend() = default;
  ~PortAudioBackend() override = default;

  std::vector<Device> getOutputDevices() const override{};

  std::vector<Device> getInputDevices() const override {}

  bell::Result<Device> getDefaultOutputDevice() const override {}

  bell::Result<Device> getDefaultInputDevice() const override {}

  bell::Result<> openStream(StreamType streamType, const AudioFormat& format,
                            unsigned int sampleRate, uint32_t bufferFrames,
                            AudioCallback callback,
                            const std::any& backendSpecificOptions) override {}

  void closeStream() override {}

  bell::Result<> startStream() override {}

  bell::Result<> stopStream() override {}

  bool isStreamOpen() const override {}

  bool isStreamRunning() const override {}

  SampleRate getStreamSampleRate() const override {}

  double getStreamLatency() const override {}
};
