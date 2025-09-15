#include "bell/audio/OggContainer.h"
#include "bell/Logger.h"
#include "bell/Result.h"
#include "bell/audio/Common.h"
#include "ogg/ogg.h"

#include <algorithm>  // For std::lower_bound

using namespace bell::audio;

namespace {
const long bufferInLen = 8 * 1024;

const int64_t seekTableStride = 64 * 1024;

const int64_t seekTableReadSize = 32 * 1024;
}  // namespace

bell::Result<> OggContainer::openForRead(
    std::shared_ptr<bell::io::DataStream> dataStream) {
  stream = std::move(dataStream);
  ogg_sync_init(&oggSyncState);

  // Read the first page to get the stream serial number
  auto firstPageRes = readNextPage();
  if (!firstPageRes) {
    BELL_LOG(error, "OggContainer", "Could not read the first Ogg page.");
    return tl::make_unexpected(audio::Errc::InvalidFormat);
  }
  streamSerialNo = ogg_page_serialno(&oggPage);

  // If the stream is seekable, build the seek table and find the total frames.
  if (stream->isSeekable()) {
    auto streamSizeRes = stream->size();
    if (streamSizeRes && *streamSizeRes > 0) {
      // This builds the table and leaves the stream position at the end.
      buildSeekTable(*streamSizeRes);
      if (!seekTable.empty()) {
        totalFrames = seekTable.back().first;
      }
    }
  }

  if (!stream->seek(0)) {
    BELL_LOG(error, "OggContainer",
             "Failed to seek to beginning of stream after setup.");
    return bell::make_unexpected_errc(std::errc::io_error);
  }

  // Reset all Ogg state to start fresh for reading.
  ogg_sync_reset(&oggSyncState);
  ogg_stream_init(&oggStreamState, streamSerialNo);

  // Read the first page again to prime the decoder.
  firstPageRes = readNextPage();
  if (!firstPageRes) {
    return tl::make_unexpected(audio::Errc::InvalidFormat);
  }

  if (ogg_stream_pagein(&oggStreamState, &oggPage) < 0) {
    BELL_LOG(error, "OggContainer", "ogg_stream_pagein failed on first page.");
    return tl::make_unexpected(audio::Errc::CodecError);
  }

  BELL_LOG(info, "OggContainer",
           "Opened Ogg stream with serial {}, total frames: {}", streamSerialNo,
           totalFrames);
  return {};
}

bell::Result<> OggContainer::readNextPage() {
  while (ogg_sync_pageout(&oggSyncState, &oggPage) != 1) {
    // Check if we are likely at the end of the stream.
    if (stream->size() && stream->position() >= *stream->size()) {
      return tl::make_unexpected(audio::Errc::CodecError);
    }

    char* buffer = ogg_sync_buffer(&oggSyncState, bufferInLen);
    if (!buffer) {
      BELL_LOG(error, "OggContainer", "ogg_sync_buffer returned null.");
      return tl::make_unexpected(audio::Errc::CodecError);
    }

    auto readRes =
        stream->read(reinterpret_cast<std::byte*>(buffer), bufferInLen);

    if (!readRes) {
      // A read error occurred.
      return tl::make_unexpected(readRes.error());
    }
    if (*readRes == 0) {
      // End of stream. Return a specific error to signal this.
      return tl::make_unexpected(audio::Errc::CodecError);
    }

    ogg_sync_wrote(&oggSyncState, *readRes);
  }
  return {};
}

void OggContainer::buildSeekTable(int64_t streamSize) {
  seekTable.clear();
  long lastGranulePos = -1;

  BELL_LOG(debug, "OggContainer",
           "Building sparse seek table with stride {} and read size {}...",
           seekTableStride, seekTableReadSize);

  int64_t currentPos = 0;
  while (currentPos < streamSize) {
    // 1. Perform a coarse seek to the next sampling point. This is the HTTP request.
    if (!stream->seek(currentPos)) {
      BELL_LOG(error, "OggContainer",
               "Failed to seek to position {} for table build.", currentPos);
      break;
    }
    ogg_sync_reset(&oggSyncState);

    // Maximum scan position
    const size_t scanEndPosition = currentPos + seekTableReadSize;

    while (stream->position() < scanEndPosition) {
      auto pageRes = readNextPage();
      if (!pageRes) {
        // We hit an error or the end of the file, stop scanning this chunk.
        break;
      }

      long granulepos = ogg_page_granulepos(&oggPage);
      if (granulepos >= 0 && granulepos > lastGranulePos) {
        // Foud a page, store the position we seeked to as it is a safe point before this page.
        seekTable.emplace_back(granulepos, currentPos);
        lastGranulePos = granulepos;
        break;
      }
    }

    currentPos += seekTableStride;
  }

  BELL_LOG(debug, "OggContainer", "Sparse seek table built with {} entries.",
           seekTable.size());
}

bell::Result<EncodedPacket> OggContainer::readNextPacket() {
  while (ogg_stream_packetout(&oggStreamState, &packet) != 1) {
    if (auto pageRes = readNextPage(); !pageRes) {
      // Propagate EndOfStream or other errors from readNextPage.
      return tl::make_unexpected(pageRes.error());
    }
    if (ogg_stream_pagein(&oggStreamState, &oggPage) < 0) {
      BELL_LOG(warn, "OggContainer",
               "ogg_stream_pagein failed. Possible data corruption.");
      return tl::make_unexpected(audio::Errc::CodecError);
    }
  }

  EncodedPacket frame;
  frame.data = {reinterpret_cast<std::byte*>(packet.packet),
                static_cast<size_t>(packet.bytes)};
  frame.streamIdx = 0;
  if (packet.granulepos > 0) {
    currentFrame = packet.granulepos;
  }
  frame.timestamp = currentFrame;

  return frame;
}

bell::Result<> OggContainer::seekToFrame(size_t frameIndex,
                                         size_t allowedDistance) {
  if (seekTable.empty()) {
    BELL_LOG(error, "OggContainer",
             "Cannot seek: stream is not seekable or has no seek table.");
    return tl::make_unexpected(audio::Errc::OperationNotSupported);
  }

  // Find the last entry whose frame number is less than or equal to the target.
  auto it = std::lower_bound(seekTable.begin(), seekTable.end(), frameIndex,
                             [](const std::pair<uint64_t, int64_t>& entry,
                                size_t value) { return entry.first < value; });

  // Step back one entry if possible
  if (it != seekTable.begin()) {
    --it;
  }

  auto res = stream->seek(it->second);
  if (!res) {
    return tl::make_unexpected(res.error());
  }

  // Reset state to begin reading from the new position.
  ogg_sync_reset(&oggSyncState);
  ogg_stream_reset(&oggStreamState);

  // Fine-grained seek: Read packets until we are at or past the target.
  while (true) {
    auto packetRes = readNextPacket();
    if (!packetRes) {
      // We hit an error or EOF before finding a suitable frame.
      BELL_LOG(warn, "OggContainer",
               "Seek failed: hit end of stream while searching for frame {}.",
               frameIndex);
      return tl::make_unexpected(packetRes.error());
    }

    // Check if the current frame is our target or past it.
    if (currentFrame >= frameIndex) {
      break;
    }

    // Check if we are within the user-defined allowed distance.
    if ((frameIndex - currentFrame) <= allowedDistance) {
      break;
    }
  }

  return {};
}

void OggContainer::close() {
  ogg_stream_destroy(&oggStreamState);
  ogg_sync_destroy(&oggSyncState);
}

uint64_t OggContainer::tellFrame() const {
  return currentFrame;
}

uint64_t OggContainer::getTotalFrames() const {
  return totalFrames;
}
