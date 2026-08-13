#pragma once

// Standard includes
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "bell/audio/Common.h"
#include "bell/audio/Container.h"

namespace bell::audio {

// Frame demuxer for MPEG-1/2/2.5 Layer III ("MP3") - each frame is
// self-delimiting via its own 4-byte header.
class Mp3Container : public Container {
 public:
  Mp3Container() = default;
  ~Mp3Container() override = default;

  bell::Result<> openForRead(
      std::shared_ptr<bell::io::DataStream> dataStream) override;
  bell::Result<EncodedPacket> readNextPacket() override;
  bell::Result<> seekToFrame(size_t frameIndex,
                             size_t allowedDistance = 0) override;
  uint64_t tellFrame() const override;
  uint64_t getTotalFrames() override;

 private:
  const char* LOG_TAG = "Mp3Container";

  struct FrameHeader {
    int sampleRate;
    int samplesPerFrame;
    int bitrateKbps;
    int frameLengthBytes;
  };

  std::shared_ptr<bell::io::DataStream> stream;
  size_t dataStartOffset = 0;
  uint64_t currentFrame = 0;
  // 0 if the stream size (or a first frame to estimate from) isn't known.
  uint64_t totalFrames = 0;
  int samplesPerFrame = 1152;
  // From the first frame parsed; basis for seekToFrame()'s CBR estimate.
  int averageBitrateKbps = 0;

  std::vector<std::byte> packetBuf;

  static std::optional<FrameHeader> parseHeader(const std::byte* buf,
                                                size_t available);
  bell::Result<> readExact(std::byte* dest, size_t size);
  bell::Result<> skipId3v2();
  // Scans forward from the current position for the next frame header,
  // cross-checked against the following frame to reject a lone false
  // sync match, and leaves the stream positioned at its first byte.
  bell::Result<FrameHeader> resync();
};
}  // namespace bell::audio

namespace bell {
using Mp3Container = audio::Mp3Container;
}  // namespace bell
