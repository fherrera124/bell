#include "bell/audio/Mp3Container.h"

// Standard includes
#include <algorithm>
#include <array>
#include <cstdint>

// Own includes
#include "bell/Logger.h"

using namespace bell::audio;

namespace {
constexpr size_t kResyncScanWindow = 8 * 1024;

const int kBitrateTableV1L3[16] = {0,   32,  40,  48,  56,  64,  80,  96,
                                   112, 128, 160, 192, 224, 256, 320, 0};
const int kBitrateTableV2L3[16] = {0,  8,   16,  24,  32,  40,  48,  56,
                                   64, 80,  96,  112, 128, 144, 160, 0};
const int kSampleRateTable[3][3] = {
    {44100, 48000, 32000},  // MPEG1
    {22050, 24000, 16000},  // MPEG2
    {11025, 12000, 8000},   // MPEG2.5
};
}  // namespace

std::optional<Mp3Container::FrameHeader> Mp3Container::parseHeader(
    const std::byte* rawBuf, size_t available) {
  if (available < 4) {
    return std::nullopt;
  }
  auto* buf = reinterpret_cast<const uint8_t*>(rawBuf);
  if (buf[0] != 0xFF || (buf[1] & 0xE0) != 0xE0) {
    return std::nullopt;
  }

  int version = (buf[1] >> 3) & 0x3;
  int layer = (buf[1] >> 1) & 0x3;
  if (layer != 1) {
    return std::nullopt;  // only Layer III ("MP3") is supported
  }

  int versionRow =
      version == 3 ? 0 : version == 2 ? 1 : version == 0 ? 2 : -1;
  if (versionRow < 0) {
    return std::nullopt;  // version==1 is reserved
  }

  int bitrateIdx = (buf[2] >> 4) & 0xF;
  int sampleRateIdx = (buf[2] >> 2) & 0x3;
  if (bitrateIdx == 0 || bitrateIdx == 15 || sampleRateIdx == 3) {
    return std::nullopt;  // free/reserved bitrate, or reserved sample rate
  }
  bool padding = (buf[2] >> 1) & 0x1;

  int bitrateKbps = version == 3 ? kBitrateTableV1L3[bitrateIdx]
                                : kBitrateTableV2L3[bitrateIdx];
  int sampleRate = kSampleRateTable[versionRow][sampleRateIdx];
  int samplesPerFrame = version == 3 ? 1152 : 576;
  int slotFactor = version == 3 ? 144 : 72;
  int frameLengthBytes =
      (slotFactor * bitrateKbps * 1000) / sampleRate + (padding ? 1 : 0);

  return FrameHeader{sampleRate, samplesPerFrame, bitrateKbps,
                     frameLengthBytes};
}

bell::Result<> Mp3Container::readExact(std::byte* dest, size_t size) {
  size_t got = 0;
  while (got < size) {
    auto readRes = stream->read(dest + got, size - got);
    if (!readRes) {
      return nonstd::make_unexpected(readRes.error());
    }
    if (*readRes == 0) {
      return nonstd::make_unexpected(Errc::EndOfStream);
    }
    got += *readRes;
  }
  return {};
}

bell::Result<> Mp3Container::skipId3v2() {
  std::array<std::byte, 10> tagHeader{};
  auto readRes = readExact(tagHeader.data(), tagHeader.size());
  dataStartOffset = 0;
  if (readRes) {
    auto* h = reinterpret_cast<const uint8_t*>(tagHeader.data());
    if (h[0] == 'I' && h[1] == 'D' && h[2] == '3') {
      // Sync-safe 28-bit size (top bit of each byte unused).
      uint32_t tagSize = (h[6] & 0x7Fu) << 21 | (h[7] & 0x7Fu) << 14 |
                         (h[8] & 0x7Fu) << 7 | (h[9] & 0x7Fu);
      dataStartOffset = 10 + tagSize;
    }
  }
  return stream->seek(dataStartOffset, bell::io::DataStream::SeekOrigin::Begin);
}

bell::Result<Mp3Container::FrameHeader> Mp3Container::resync() {
  std::vector<std::byte> scanBuf(kResyncScanWindow);
  size_t scanStart = stream->position();
  auto readRes = stream->read(scanBuf.data(), scanBuf.size());
  if (!readRes || *readRes < 4) {
    return nonstd::make_unexpected(Errc::EndOfStream);
  }
  size_t available = *readRes;

  for (size_t i = 0; i + 4 <= available; i++) {
    auto header = parseHeader(scanBuf.data() + i, available - i);
    if (!header) {
      continue;
    }
    if (i + static_cast<size_t>(header->frameLengthBytes) + 4 <= available &&
        !parseHeader(scanBuf.data() + i + header->frameLengthBytes, 4)) {
      continue;  // lone sync match - the next frame doesn't line up
    }
    if (auto seekRes = stream->seek(scanStart + i,
                                    bell::io::DataStream::SeekOrigin::Begin);
        !seekRes) {
      return nonstd::make_unexpected(seekRes.error());
    }
    return *header;
  }

  return nonstd::make_unexpected(Errc::InvalidFormat);
}

bell::Result<> Mp3Container::openForRead(
    std::shared_ptr<bell::io::DataStream> dataStream) {
  stream = std::move(dataStream);

  if (!stream->isSeekable()) {
    return nonstd::make_unexpected(Errc::OperationNotSupported);
  }

  if (auto res = skipId3v2(); !res) {
    return nonstd::make_unexpected(res.error());
  }

  auto header = resync();
  if (!header) {
    BELL_LOG(error, LOG_TAG, "No valid MP3 frame found: {}", header.error());
    return nonstd::make_unexpected(header.error());
  }

  samplesPerFrame = header->samplesPerFrame;
  averageBitrateKbps = header->bitrateKbps;

  if (auto size = stream->size(); size && *size > dataStartOffset) {
    uint64_t dataBytes = *size - dataStartOffset;
    totalFrames = (dataBytes * 8 * static_cast<uint64_t>(header->sampleRate)) /
                 (static_cast<uint64_t>(averageBitrateKbps) * 1000);
  }

  return {};
}

bell::Result<EncodedPacket> Mp3Container::readNextPacket() {
  std::array<std::byte, 4> headerBytes{};
  size_t framePos = stream->position();
  if (auto res = readExact(headerBytes.data(), headerBytes.size()); !res) {
    return nonstd::make_unexpected(res.error());
  }

  auto header = parseHeader(headerBytes.data(), headerBytes.size());
  if (!header) {
    if (auto seekRes = stream->seek(framePos, bell::io::DataStream::SeekOrigin::Begin);
        !seekRes) {
      return nonstd::make_unexpected(seekRes.error());
    }
    auto resyncRes = resync();
    if (!resyncRes) {
      return nonstd::make_unexpected(resyncRes.error());
    }
    header = *resyncRes;
    if (auto res = readExact(headerBytes.data(), headerBytes.size()); !res) {
      return nonstd::make_unexpected(res.error());
    }
  }

  packetBuf.resize(static_cast<size_t>(header->frameLengthBytes));
  std::copy(headerBytes.begin(), headerBytes.end(), packetBuf.begin());
  if (auto res = readExact(packetBuf.data() + 4, packetBuf.size() - 4); !res) {
    return nonstd::make_unexpected(res.error());
  }

  samplesPerFrame = header->samplesPerFrame;
  currentFrame += static_cast<uint64_t>(samplesPerFrame);

  EncodedPacket packet;
  packet.data = {packetBuf.data(), packetBuf.size()};
  packet.streamIdx = 0;
  packet.timestamp = static_cast<uint32_t>(currentFrame);
  return packet;
}

bell::Result<> Mp3Container::seekToFrame(size_t frameIndex, size_t) {
  if (!stream->isSeekable() || !stream->size() || averageBitrateKbps == 0) {
    return nonstd::make_unexpected(Errc::OperationNotSupported);
  }

  uint64_t target =
      totalFrames > 0 ? std::min<uint64_t>(frameIndex, totalFrames) : frameIndex;

  uint64_t dataBytes = *stream->size() - dataStartOffset;
  uint64_t targetByte = dataStartOffset;
  if (totalFrames > 0) {
    targetByte += (target * dataBytes) / totalFrames;
  }

  if (auto seekRes = stream->seek(targetByte, bell::io::DataStream::SeekOrigin::Begin);
      !seekRes) {
    return nonstd::make_unexpected(seekRes.error());
  }

  auto header = resync();
  if (!header) {
    return nonstd::make_unexpected(header.error());
  }

  currentFrame = target;
  samplesPerFrame = header->samplesPerFrame;
  return {};
}

uint64_t Mp3Container::tellFrame() const {
  return currentFrame;
}

uint64_t Mp3Container::getTotalFrames() {
  return totalFrames;
}
