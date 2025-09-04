#include "bell/http/DataStream.h"
#include <cassert>
#include "bell/Logger.h"
#include "bell/http/Client.h"
#include "bell/http/Common.h"

using namespace bell::http;

bell::Result<> DataStream::open(bell::HTTPMethod method, const std::string& url,
                                const Headers& headers) {
  // Ensure the chunk size is valid
  lastReadChunk.resize(chunkSize);
  bytesInLastReadChunk = 0;
  chunkStartPosition = 0;

  auto req = Request::create(method, url);
  if (!req) {
    return tl::make_unexpected(req.error());
  }

  req->headers = headers;
  this->httpRequest = *req;

  return requestNextRange();
}

bool DataStream::isSeekable() const {
  return isSeekableFlag;
}

bool DataStream::isInfinite() const {
  return !totalSize.has_value();
}

std::optional<size_t> DataStream::size() const {
  return totalSize;
}

size_t DataStream::position() const {
  return currentPosition;
}

bell::Result<> DataStream::seek(size_t offset) {
  if (!isSeekable()) {
    return bell::make_unexpected_errc<>(std::errc::invalid_argument);
  }

  if (offset >= totalSize.value_or(0)) {
    return bell::make_unexpected_errc<>(std::errc::invalid_seek);
  }

  currentPosition = offset;
  bytesInLastReadChunk = 0;
  chunkStartPosition = 0;

  return {};
}

bell::Result<size_t> DataStream::read(std::byte* outputBuffer,
                                      size_t outputBufferLen) {
  size_t totalCopied = 0;
  size_t toRead = outputBufferLen;

  while (toRead > 0) {
    // Copy remaining bytes from current chunk
    size_t availableInChunk = 0;
    if (bytesInLastReadChunk > chunkStartPosition) {
      availableInChunk = bytesInLastReadChunk - chunkStartPosition;
    }

    if (availableInChunk > 0) {
      size_t toCopy = std::min(toRead, availableInChunk);
      std::copy(lastReadChunk.data() + chunkStartPosition,
                lastReadChunk.data() + chunkStartPosition + toCopy,
                outputBuffer + totalCopied);
      chunkStartPosition += toCopy;
      currentPosition += toCopy;
      totalCopied += toCopy;
      toRead -= toCopy;

      // If we've satisfied the request, return
      if (toRead == 0) {
        break;
      }
    }

    // No more data in current chunk, request next
    if (isSeekable()) {
      auto res = requestNextRange();
      if (!res) {
        return bell::make_unexpected_errc<size_t>(std::errc::io_error);
      }
    } else {
      /*
      // For non-seekable streams, just read next chunk from the same connection
      auto* stream = connection->getResponse()->getStream();
      stream->read(reinterpret_cast<char*>(lastReadChunk.data()), chunkSize);
      if (stream->fail() && !stream->eof()) {
        return bell::make_unexpected_errc<size_t>(std::errc::io_error);
      }
      bytesInLastReadChunk = static_cast<size_t>(stream->gcount());
      chunkStartPosition = 0;

      if (bytesInLastReadChunk == 0) {
        // EOF for finite streams
        break;
      }
      */
    }

    // If after fetching, no bytes were read, break (EOF for finite streams)
    if (bytesInLastReadChunk == 0) {
      break;
    }

    // Reset chunk position for next read iteration
    chunkStartPosition = 0;
  }

  return totalCopied;
}

bell::Result<> DataStream::requestNextRange() {

  size_t chunkReadSize =
      std::min(chunkSize, totalSize.value_or(SIZE_MAX) - currentPosition);

  // Range starts exactly at currentPosition;
  httpRequest.headers["Content-Range"] = fmt::format(
      "bytes {}-{}", currentPosition, currentPosition + chunkReadSize - 1);

  auto response = httpClient->rawRequest(httpRequest);
  if (!response) {
    return tl::make_unexpected(response.error());
  }

  totalSize = response->contentLength;

  if (response->headers.contains("Content-Range")) {
    auto rangeHeader = response->headers.at("Content-Range");
    // Parse the Content-Range header
    // Example: "bytes 0-1023/2048"
    auto rangeHeaderItr = rangeHeader.find('/');

    auto totalSizeStr = rangeHeader.substr(rangeHeader.find('/') + 1);
    if (rangeHeaderItr != std::string::npos) {
      try {
        totalSize = std::stoll(totalSizeStr.data());
        isSeekableFlag = true;
      } catch (const std::invalid_argument& e) {
        BELL_LOG(error, LOG_TAG, "Failed to parse Content-Range header: {}",
                 e.what());
        return bell::make_unexpected_errc<>(std::errc::bad_message);
      }
    } else {
      isSeekableFlag = false;
    }
  }

  size_t toRead = *response->contentLength;
  auto* stream = response->stream();
  assert(toRead <= lastReadChunk.size());

  stream->read(reinterpret_cast<char*>(lastReadChunk.data()), toRead);
  if (stream->fail() && !stream->eof()) {
    return bell::make_unexpected_errc<>(std::errc::io_error);
  }

  bytesInLastReadChunk = static_cast<size_t>(stream->gcount());
  chunkStartPosition = 0;

  return {};
}
