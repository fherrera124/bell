#pragma once

// Standard includes
#include <cstddef>
#include <cstdint>
#include <memory>

#include "bell/audio/Common.h"

// Common headers
#include "bell/Result.h"
#include "bell/io/DataStream.h"

namespace bell::audio {
struct ContainerStreamInfo {
  int streamIndex;
};

/**
 * @brief Base interface for audio container formats (e.g., OGG, MP4, WAV)
 */
class Container {
 public:
  Container() = default;
  virtual ~Container() = default;

  /**
     * @brief Opens the container for reading
     *
     * @param source Raw byte stream or file path (implementation-defined)
     * @return Result of the operation
     */
  virtual bell::Result<> openForRead(
      std::shared_ptr<bell::io::DataStream> dataStream) = 0;

  virtual bell::Result<EncodedPacket> readNextPacket() = 0;

  /**
     * @brief Seeks to a specific frame number
     *
     * @param frameIndex Frame index (0-based)
     * @param allowedDistance Maximum distance to seek in frames
     * @return Result indicating success or error
     */
  virtual bell::Result<> seekToFrame(size_t frameIndex,
                                     size_t allowedDistance = 0) = 0;

  /**
   * @brief Seeks directly to a known byte offset, given the frame number expected there
   *
   * Optional fast path for containers that can map an external seek-table
   * lookup (or a bitrate-based estimate) straight to a byte offset,
   * skipping the bisection search seekToFrame() would otherwise need.
   * Default implementation reports unsupported - callers needing a
   * fallback should use seekToFrame() instead.
   *
   * @param byteOffset Byte offset to seek to
   * @param targetFrame Frame index expected at (or near) byteOffset, used to fine-tune afterwards
   * @param allowedDistance Maximum acceptable distance from targetFrame, in frames
   * @return Result indicating success, or Errc::OperationNotSupported if this container doesn't support it
   */
  virtual bell::Result<> seekToByteOffset(size_t byteOffset,
                                          uint64_t targetFrame,
                                          size_t allowedDistance = 0) {
    return nonstd::make_unexpected(Errc::OperationNotSupported);
  }

  /**
   * @brief Gets the current playback position in milliseconds
   */
  virtual uint64_t tellFrame() const = 0;

  /**
   * @brief Gets the duration of the audio stream in milliseconds
   */
  virtual uint64_t getTotalFrames() = 0;
};

}  // namespace bell::audio

namespace bell {
using AudioContainer = audio::Container;
}
