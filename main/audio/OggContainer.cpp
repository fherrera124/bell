#include "bell/audio/OggContainer.h"
#include <iostream>
#include "bell/Logger.h"
#include "bell/Result.h"
#include "bell/audio/Common.h"
#include "ogg/ogg.h"

using namespace bell::audio;

namespace {
const long bufferInLen = 1024;
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

  BELL_LOG(info, LOG_TAG, "Opened Ogg stream with serial number {}",
           ogg_page_serialno(&oggPage));

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

void OggContainer::close() {
  ogg_stream_destroy(&oggStreamState);
  ogg_sync_destroy(&oggSyncState);
}
