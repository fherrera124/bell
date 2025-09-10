#include "bell/audio/OggContainer.h"
#include <iostream>
#include "bell/Logger.h"
#include "bell/Result.h"
#include "bell/audio/Common.h"
#include "ogg/ogg.h"

using namespace bell::audio;

namespace {
const long bufferInLen = 4 * 1024;
}

bell::Result<> OggContainer::openForRead(
    std::shared_ptr<bell::io::DataStream> dataStream) {
  stream = std::move(dataStream);

  ogg_sync_init(&oggSyncState);

  auto res = readNextPage();
  if (!res) {
    return tl::make_unexpected(audio::Errc::InvalidFormat);
  }

  streamSerialNo = ogg_page_serialno(&oggPage);
  ogg_stream_init(&oggStreamState, streamSerialNo);

  if (ogg_stream_pagein(&oggStreamState, &oggPage) < 0) {
    // Could not feed the the first page
    return tl::make_unexpected(audio::Errc::CodecError);
  }

  // To get total frames, we need to seek to the end, find the last page's
  // granulepos, and then seek back.
  auto streamSizeRes = stream->size();
  if (streamSizeRes && *streamSizeRes > 0) {
    // Seek near the end to find the last page
    // We look a few buffers away from the end to ensure we don't miss the page header
    int64_t searchPos = *streamSizeRes - (bufferInLen * 2);
    if (searchPos < 0)
      searchPos = 0;

    (void)stream->seek(searchPos);
    ogg_sync_reset(&oggSyncState);

    long lastGranulepos = -1;
    while (readNextPage()) {
      long currentGranulepos = ogg_page_granulepos(&oggPage);
      if (currentGranulepos > lastGranulepos) {
        lastGranulepos = currentGranulepos;
      }
    }
    if (lastGranulepos > 0) {
      totalFrames = lastGranulepos;
    }
  }

  // Reset stream state to the beginning
  (void)stream->seek(0);
  ogg_sync_reset(&oggSyncState);
  ogg_stream_reset(&oggStreamState);

  // Re-read the first page to prime the decoder for reading
  (void)readNextPage();
  ogg_stream_pagein(&oggStreamState, &oggPage);

  BELL_LOG(info, "OggContainer",
           "Opened Ogg stream with serial {}, total frames: {}", streamSerialNo,
           totalFrames);

  return {};
}

bell::Result<> OggContainer::readNextPage() {
  while (true) {
    int ret = ogg_sync_pageout(&oggSyncState, &oggPage);
    if (ret == 1)
      return {};  // Found a page

    char* bufIn = ogg_sync_buffer(&oggSyncState, bufferInLen);
    auto res = stream->read(reinterpret_cast<std::byte*>(bufIn), bufferInLen);
    if (!res) {
      return tl::make_unexpected(res.error());
    }

    // Notify Ogg of the new data
    ogg_sync_wrote(&oggSyncState, *res);
  }

  return {};
}

bell::Result<EncodedPacket> OggContainer::readNextPacket() {
  int ret;
  // Try to get the next packet from the current page
  while ((ret = ogg_stream_packetout(&oggStreamState, &packet)) != 1) {
    if (ret < 0) {
      // Synchronization lost or other error
      return tl::make_unexpected(audio::Errc::CodecError);
    }

    // Need to read more pages
    auto pageRes = readNextPage();
    if (!pageRes) {
      return tl::make_unexpected(pageRes.error());
    }

    // Feed the new page to the stream
    if (ogg_stream_pagein(&oggStreamState, &oggPage) < 0) {
      return tl::make_unexpected(audio::Errc::CodecError);
    }
  }

  // Create EncodedAudioFrame from the packet
  EncodedPacket frame;
  frame.data = {reinterpret_cast<std::byte*>(packet.packet),
                static_cast<size_t>(packet.bytes)};
  frame.streamIdx = 0;
  frame.timestamp = packet.granulepos;
  // TODO: calculate duration

  return frame;
}

bell::Result<> OggContainer::seekToFrame(size_t frameIndex,
                                         size_t allowedDistance) {
  auto streamSizeRes = stream->size();
  if (!streamSizeRes) {
    BELL_LOG(error, "OggContainer", "Cannot seek: stream is not seekable.");
    return tl::make_unexpected(audio::Errc::OperationNotSupported);
  }

  int64_t low_offset = 0;
  int64_t high_offset = *streamSizeRes;
  int64_t best_offset = 0;

  for (int i = 0; i < 16 && low_offset < high_offset; ++i) {
    int64_t mid_offset = low_offset + (high_offset - low_offset) / 2;
    auto res = stream->seek(mid_offset);
    if (!res) {
      return tl::make_unexpected(res.error());
    }

    ogg_sync_reset(&oggSyncState);

    long current_granule = -1;
    while (true) {
      auto pageRes = readNextPage();
      if (!pageRes) {
        high_offset = mid_offset;
        current_granule = -2;
        break;
      }
      current_granule = ogg_page_granulepos(&oggPage);
      if (current_granule >= 0)
        break;
    }
    if (current_granule == -2)
      continue;

    size_t distance = (current_granule > (long)frameIndex)
                          ? (current_granule - frameIndex)
                          : (frameIndex - current_granule);

    if (distance <= allowedDistance) {
      best_offset = mid_offset;
      break;
    }

    if (static_cast<size_t>(current_granule) < frameIndex) {
      low_offset = mid_offset + 1;
      best_offset = mid_offset;
    } else {
      high_offset = mid_offset;
    }
  }

  BELL_LOG(info, "OggContainer", "Seeking to best offset {}", best_offset);
  auto res = stream->seek(best_offset);
  if (!res) {
    return tl::make_unexpected(res.error());
  }

  ogg_sync_reset(&oggSyncState);
  ogg_stream_reset(&oggStreamState);

  while (true) {
    auto pageRes = readNextPage();
    if (!pageRes)
      return tl::make_unexpected(pageRes.error());

    if (ogg_page_serialno(&oggPage) != static_cast<int>(streamSerialNo))
      continue;

    long page_granule = ogg_page_granulepos(&oggPage);
    if (page_granule >= 0 && static_cast<size_t>(page_granule) >= frameIndex) {
      ogg_stream_pagein(&oggStreamState, &oggPage);
      break;
    }
    ogg_stream_pagein(&oggStreamState, &oggPage);
  }

  while (true) {
    ogg_packet temp_packet;
    int ret = ogg_stream_packetpeek(&oggStreamState, &temp_packet);
    if (ret == 0)
      break;
    if (ret < 0) {
      ogg_stream_packetout(&oggStreamState, &temp_packet);
      continue;
    }

    if (temp_packet.granulepos > 0 &&
        static_cast<size_t>(temp_packet.granulepos) < frameIndex) {
      ogg_stream_packetout(&oggStreamState, &temp_packet);
    } else {
      // Update current frame position after seek is complete
      if (temp_packet.granulepos > 0) {
        currentFrame = temp_packet.granulepos;
      }
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
