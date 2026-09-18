
#include "bell/http/DataStream.h"

#include <cassert>

#include "bell/Logger.h"
#include "bell/http/Client.h"
#include "bell/http/Common.h"

using namespace bell::http;

namespace {
constexpr int kMaxRedirects = 5;

bool isRedirectStatus(int statusCode) {
  return statusCode == 301 || statusCode == 302 || statusCode == 303 ||
         statusCode == 307 || statusCode == 308;
}
}  // namespace

bell::Result<> DataStream::open(bell::HTTPMethod method, const std::string& url,
                                const Headers& headers) {
  if (chunkSize == 0) {
    return bell::make_unexpected_errc<>(std::errc::invalid_argument);
  }
  pendingReadError.clear();
  totalSize.reset();
  lastReadChunk.resize(chunkSize);
  bytesInLastReadChunk = 0;
  chunkStartPosition = 0;
  currentPosition = 0;
  isSeekableFlag = false;
  activeResponse.reset();

  std::string requestUrl = url;
  bell::Result<Response> response =
      bell::make_unexpected_errc<Response>(std::errc::io_error);

  for (int redirectCount = 0;; redirectCount++) {
    auto req = Request::create(method, requestUrl);
    if (!req) {
      return nonstd::make_unexpected(req.error());
    }

    req->operationTimeoutMs = 3000;
    req->headers = headers;
    this->httpRequest = *req;
    this->httpRequest.headers["Connection"] = "keep-alive";
    this->httpRequest.headers["Range"] =
        fmt::format("bytes=0-{}", chunkSize - 1);

    response = httpClient->rawRequest(httpRequest);
    if (!response) {
      return nonstd::make_unexpected(response.error());
    }

    if (!isRedirectStatus(response->statusCode) ||
        !response->headers.contains("Location")) {
      break;
    }
    if (redirectCount >= kMaxRedirects) {
      BELL_LOG(error, LOG_TAG, "Too many redirects following {}", url);
      return bell::make_unexpected_errc<>(std::errc::too_many_links);
    }
    requestUrl = response->headers.at("Location");
    BELL_LOG(debug, LOG_TAG, "Following {} redirect to {}",
             response->statusCode, requestUrl);
  }

  totalSize = response->contentLength;
  activeResponse = std::move(*response);

  // Detect seekability via Content-Range
  if (activeResponse->headers.contains("Content-Range")) {
    auto rangeHeader = activeResponse->headers.at("Content-Range");
    auto slashPos = rangeHeader.find('/');
    if (slashPos != std::string::npos) {
      try {
        totalSize = std::stoll(rangeHeader.substr(slashPos + 1));
        isSeekableFlag = true;
      } catch (const std::invalid_argument& e) {
        BELL_LOG(error, LOG_TAG, "Failed to parse Content-Range header: {}",
                 e.what());
        return bell::make_unexpected_errc<>(std::errc::bad_message);
      }
    }
  } else if (totalSize.has_value()) {
    // If Content-Length exists but no Content-Range → finite, not seekable
    isSeekableFlag = false;
  }

  return readChunk();
}

bell::Result<> DataStream::readChunk() {
  auto result = activeResponse->readBodyChunk(lastReadChunk.data(), chunkSize);
  if (!result) {
    pendingReadError = result.error();
    return nonstd::make_unexpected(result.error());
  }
  bytesInLastReadChunk = *result;
  chunkStartPosition = 0;
  return {};
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

bell::Result<> DataStream::seek(size_t offset, SeekOrigin origin) {
  (void)origin;  // TODO: support other origins
  if (!isSeekable()) {
    return bell::make_unexpected_errc<>(std::errc::invalid_argument);
  }
  if (offset >= totalSize.value_or(0)) {
    return bell::make_unexpected_errc<>(std::errc::invalid_seek);
  }

  pendingReadError.clear();
  activeResponse.reset();
  currentPosition = offset;
  bytesInLastReadChunk = 0;
  chunkStartPosition = 0;

  return {};
}

bell::Result<size_t> DataStream::read(std::byte* outputBuffer,
                                      size_t outputBufferLen) {
  if (outputBufferLen == 0) return size_t{0};
  if (pendingReadError) return nonstd::make_unexpected(pendingReadError);
  size_t totalCopied = 0;
  while (totalCopied < outputBufferLen) {
    const size_t available = bytesInLastReadChunk - chunkStartPosition;
    if (available > 0) {
      const size_t count = std::min(outputBufferLen - totalCopied, available);
      std::copy_n(lastReadChunk.data() + chunkStartPosition, count,
                  outputBuffer + totalCopied);
      chunkStartPosition += count;
      currentPosition += count;
      totalCopied += count;
      if (totalCopied == outputBufferLen) break;
    }

    bell::Result<> result;
    if (activeResponse) {
      result = readChunk();
    } else {
      bytesInLastReadChunk = 0;
      chunkStartPosition = 0;
    }
    if (result && bytesInLastReadChunk == 0 && isSeekable() &&
        currentPosition < totalSize.value_or(0)) {
      result = requestNextRange();
    }
    if (!result) {
      pendingReadError = result.error();
      if (totalCopied > 0) return totalCopied;
      return nonstd::make_unexpected(pendingReadError);
    }
    if (bytesInLastReadChunk == 0) break;
  }
  return totalCopied;
}

bell::Result<> DataStream::requestNextRange() {
  assert(isSeekable());

  size_t chunkReadSize =
      std::min(chunkSize, totalSize.value_or(SIZE_MAX) - currentPosition);

  if (chunkReadSize == 0) {
    return bell::make_unexpected_errc<>(std::errc::invalid_seek);
  }

  httpRequest.headers["Range"] = fmt::format(
      "bytes={}-{}", currentPosition, currentPosition + chunkReadSize - 1);

  // Reset the old response
  activeResponse.reset();
  BELL_LOG(debug, LOG_TAG, "Requesting range: {}",
           httpRequest.headers["Range"]);

  auto response = httpClient->rawRequest(httpRequest);
  if (!response) {
    return nonstd::make_unexpected(response.error());
  }

  activeResponse = std::move(*response);

  if (!activeResponse->contentLength ||
      *activeResponse->contentLength != chunkReadSize) {
    activeResponse.reset();
    return bell::make_unexpected_errc<>(std::errc::bad_message);
  }
  return readChunk();
}
