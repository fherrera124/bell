#include "bell/http/Reader.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>

#include "bell/Result.h"
#include "bell/http/Common.h"
#include "bell/io/MemoryStream.h"
#include "bell/net/SocketStream.h"
#include "bell/net/URIParser.h"
#include "nonstd/expected.hpp"

using namespace bell;

http::Reader::Reader(Direction readerDirection, std::istream* istream,
                     std::vector<char>* externalBuffer)
    : readerDirection(readerDirection),
      istream(istream),
      bufferPtr(externalBuffer) {
  if (bufferPtr == nullptr) {
    // External buffer not provided, use internal buffer
    bufferPtr = &internalBuffer;
  } else {
    bufferPtr->clear();

    usingExternalBuffer = true;
  }
}

http::Reader::Reader(Direction direction, net::SocketStream* stream,
                     std::vector<char>* externalBuffer)
    : Reader(direction, static_cast<std::istream*>(stream), externalBuffer) {
  socketStream = stream;
}

http::Reader::Reader(Direction direction,
                     std::shared_ptr<net::SocketStream> stream)
    : Reader(direction, stream.get()) {
  sharedIstream = std::move(stream);
}

http::Reader::Reader(Reader&& other) noexcept {
  swap(other);
}

http::Reader& http::Reader::operator=(Reader&& other) noexcept {
  if (this != &other) {
    Reader previous(std::move(other));
    swap(previous);
  }
  return *this;
}

void http::Reader::swap(Reader& other) noexcept {
  using std::swap;
  swap(readerDirection, other.readerDirection);
  swap(sharedIstream, other.sharedIstream);
  swap(istream, other.istream);
  swap(socketStream, other.socketStream);
  swap(internalBuffer, other.internalBuffer);
  swap(bufferPtr, other.bufferPtr);
  swap(usingExternalBuffer, other.usingExternalBuffer);
  swap(headersValid, other.headersValid);
  swap(readContentLength, other.readContentLength);
  swap(minorVersion, other.minorVersion);
  swap(bodyStartByteCount_, other.bodyStartByteCount_);
  swap(phrHeaders, other.phrHeaders);
  swap(contentLength, other.contentLength);
  swap(method, other.method);
  swap(path, other.path);
  swap(queryParams, other.queryParams);
  swap(statusCode, other.statusCode);
  swap(statusMessage, other.statusMessage);
  if (!usingExternalBuffer) bufferPtr = &internalBuffer;
  if (!other.usingExternalBuffer) other.bufferPtr = &other.internalBuffer;
}

bell::Result<> http::Reader::readHeaders() {
  return readHeaders(false);
}

bell::Result<> http::Reader::readHeaders(bool responseToHead) {
  if (!istream || readerDirection == Direction::Invalid || headersValid) {
    return make_unexpected_errc(std::errc::operation_not_permitted);
  }

  size_t numHeaders = 0;

  // Response specific
  const char* statusMessagePtr = nullptr;
  size_t statusMessageLen = 0;
  int parsedStatusCode = 0;

  // Request specific
  const char* pathPtr = nullptr;
  size_t pathLen = 0;
  const char* methodPtr = nullptr;
  size_t methodLen = 0;

  int lastPhrResult =
      0;  // Last result from phr_parse_request/phr_parse_response

  char lastChar = 0;
  size_t lastLineStart = 0;

  if (!usingExternalBuffer) {
    bufferPtr = &internalBuffer;
  }

  // Consume the stream byte by byte, so we dont read into the body
  while (lastPhrResult <= 0 && istream->get(lastChar)) {
    if (bufferPtr->size() >= maxRequestLen) {
      return make_unexpected_errc(std::errc::message_size);
    }

    bufferPtr->push_back(lastChar);

    // Full line read, process it
    if (bufferPtr->size() > 2 && bufferPtr->back() == '\n' &&
        bufferPtr->at(bufferPtr->size() - 2) == '\r') {

      // Reserve space for the headers
      phrHeaders.push_back({});
      numHeaders = phrHeaders.size();

      if (readerDirection == Direction::Request) {
        lastPhrResult =
            phr_parse_request(bufferPtr->data(), bufferPtr->size(), &methodPtr,
                              &methodLen, &pathPtr, &pathLen, &minorVersion,
                              phrHeaders.data(), &numHeaders, lastLineStart);
      } else {
        // Handle the response
        lastPhrResult = phr_parse_response(
            bufferPtr->data(), bufferPtr->size(), &minorVersion,
            &parsedStatusCode, &statusMessagePtr, &statusMessageLen,
            phrHeaders.data(), &numHeaders, lastLineStart);
      }

      bool isLastLine =
          lastLineStart > 0 && lastLineStart == bufferPtr->size() - 2;

      // Throw on phr error, or if the parser is not done yet and we're at the end
      if (lastPhrResult == -1 || (isLastLine && lastPhrResult <= 0)) {
        return make_unexpected_errc(std::errc::bad_message);
      }

      lastLineStart = bufferPtr->size();
    }
  }

  if (lastPhrResult <= 0) {
    return nonstd::make_unexpected(readFailure(
        bufferPtr->empty() ? Errc::EndOfStream : Errc::IncompleteMessage));
  }
  phrHeaders.resize(numHeaders);

  // Content-Length must be an unambiguous, nonnegative decimal size.
  size_t lengthFields = 0;
  for (const auto& header : phrHeaders) {
    constexpr std::string_view name = "Content-Length";
    if (header.name_len == name.size() &&
        std::equal(name.begin(), name.end(), header.name,
                   [](unsigned char a, unsigned char b) {
                     return std::tolower(a) == std::tolower(b);
                   })) {
      ++lengthFields;
    }
  }
  if (lengthFields > 0) {
    auto value = getHeader("Content-Length");
    size_t length = 0;
    auto parsed = std::from_chars(value.data(), value.data() + value.size(), length);
    if (lengthFields != 1 || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
      return make_unexpected_errc(std::errc::bad_message);
    }
    contentLength = length;
  } else if (readerDirection == Direction::Request) {
    contentLength = 0;
  }

  const bool noResponseBody = readerDirection == Direction::Response &&
      (responseToHead || parsedStatusCode < 200 || parsedStatusCode == 204 ||
       parsedStatusCode == 304);
  if (noResponseBody) {
    contentLength = 0;
  } else if (!getHeader("Transfer-Encoding").empty()) {
    // Transfer codings require a decoder before message boundaries are known.
    return make_unexpected_errc(std::errc::not_supported);
  }

  if (readerDirection == Direction::Response) {
    statusCode = parsedStatusCode;
    statusMessage = std::string_view(statusMessagePtr, statusMessageLen);

    if (statusCode < 100 || statusCode >= 600) {
      return make_unexpected_errc(std::errc::protocol_not_supported);
    }
  } else {
    path = std::string_view(pathPtr, pathLen);
    method = parseMethod({methodPtr, methodLen});

    if (minorVersion == 1 && getHeader("Host").empty()) {
      return make_unexpected_errc(std::errc::protocol_not_supported);
    }

    headersValid = true;
    auto res = parseQueryParams();
    if (!res) {
      headersValid = false;
      return res;
    }
  }

  headersValid = true;
  if (sharedIstream) {
    bodyStartByteCount_ = sharedIstream->totalBytesConsumed();
  }

  return {};
}

http::Reader::~Reader() {
  releaseConnection();
}

void http::Reader::releaseConnection() {
  if (readerDirection != Direction::Response || !sharedIstream) {
    return;
  }
  const size_t consumed = sharedIstream->totalBytesConsumed();
  if (!headersValid || !contentLength || consumed < bodyStartByteCount_ ||
      consumed - bodyStartByteCount_ != *contentLength ||
      sharedIstream->readState() != net::ReadState::Ready ||
      (statusCode && *statusCode < 200) || !keepAliveRequested()) {
    sharedIstream->close();
  }
}

std::error_code http::Reader::readFailure(Errc eofError) const {
  if (socketStream && socketStream->readState() == net::ReadState::Error) {
    return socketStream->readError();
  }
  if (istream->bad() || !istream->eof()) {
    return std::make_error_code(std::errc::io_error);
  }
  return make_error_code(eofError);
}

std::istream* http::Reader::getStream() const {
  return istream;
}

size_t http::Reader::getContentLength() const {
  return contentLength.value_or(0);
}

bell::Result<std::unordered_map<std::string, std::string>>
http::Reader::getQueryParams() const {
  if (!isValid(Direction::Request)) {
    return nonstd::make_unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }

  return queryParams;
}

bool http::Reader::keepAliveRequested() const {
  auto connectionHeader = getHeader("Connection");
  if (connectionHeader.empty()) {
    return minorVersion >= 1;  // HTTP/1.1 defaults to keep-alive
  }

  constexpr std::string_view kClose = "close";
  bool isClose =
      connectionHeader.size() == kClose.size() &&
      std::equal(connectionHeader.begin(), connectionHeader.end(),
                kClose.begin(), [](char a, char b) {
                  return std::tolower(a) == std::tolower(b);
                });
  return !isClose;
}

std::string_view http::Reader::getHeader(const std::string& headerName) const {
  for (const auto& header : phrHeaders) {
    if (header.name_len == headerName.size() &&
        std::equal(headerName.begin(), headerName.end(), header.name,
                   [](char a, char b) {
                     // Case insensitive comparison
                     return std::tolower(a) == std::tolower(b);
                   })) {
      return {header.value, header.value_len};
    }
  }

  return {};
}

http::Headers http::Reader::getAllHeaders() const {
  Headers headers{};
  for (const auto& header : phrHeaders) {
    headers.insert({std::string(header.name, header.name_len),
                    std::string(header.value, header.value_len)});
  }
  return headers;
}

bell::Result<std::string_view> http::Reader::getBodyStringView() {
  if (!usingExternalBuffer) {
    bufferPtr = &internalBuffer;
  }

  auto res = readBody();
  if (!res) {
    return nonstd::make_unexpected(res.error());
  }

  if (!usingExternalBuffer) {
    bufferPtr = &internalBuffer;
  }

  return std::string_view{
      bufferPtr->data() + bufferPtr->size() - readContentLength,
      readContentLength};
}

bell::Result<std::vector<std::byte>> http::Reader::getBodyBytes() {
  if (!usingExternalBuffer) {
    bufferPtr = &internalBuffer;
  }

  auto res = readBody();
  if (!res) {
    return nonstd::make_unexpected(res.error());
  }

  return std::vector<std::byte>{
      reinterpret_cast<std::byte*>(bufferPtr->data() + bufferPtr->size() -
                                   readContentLength),
      reinterpret_cast<std::byte*>(bufferPtr->data() + bufferPtr->size()),
  };
}

bell::Result<const std::byte*> http::Reader::getBodyBytesPtr() {
  if (!usingExternalBuffer) {
    bufferPtr = &internalBuffer;
  }

  auto res = readBody();
  if (!res) {
    return nonstd::make_unexpected(res.error());
  }

  return reinterpret_cast<const std::byte*>(
      bufferPtr->data() + bufferPtr->size() - readContentLength);
}

bell::Result<> http::Reader::parseQueryParams() {
  if (!isValid(Direction::Request)) {
    return make_unexpected_errc(std::errc::operation_not_permitted);
  }

  if (!path.has_value()) {
    return make_unexpected_errc(std::errc::operation_not_permitted);
  }

  auto queryStart = path->find('?');
  if (queryStart != std::string::npos) {
    io::IMemoryStream ss(
        reinterpret_cast<const std::byte*>(path->data() + queryStart + 1),
        path->size() - queryStart - 1);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
      size_t pos = pair.find('=');
      if (pos != std::string::npos) {
        std::string key = net::decodeURLEncoded(pair.substr(0, pos));
        std::string value = net::decodeURLEncoded(pair.substr(pos + 1));
        queryParams[key] = value;
      }
    }

    // Remove query parameters from the path
    path = path->substr(0, queryStart);
  }

  return {};
}

bell::Result<size_t> http::Reader::getBodyBytesLength() {
  auto res = readBody();
  if (!res) {
    return nonstd::make_unexpected(res.error());
  }

  return readContentLength;
}

size_t http::Reader::remainingBodyBytes() const {
  if (!contentLength.has_value() || readContentLength >= *contentLength) {
    return 0;
  }

  return *contentLength - readContentLength;
}

bell::Result<size_t> http::Reader::readBodyChunk(std::byte* dst, size_t len) {
  if (!isValid(readerDirection)) {
    return make_unexpected_errc<size_t>(std::errc::operation_not_permitted);
  }
  const size_t toRead = contentLength ? std::min(len, remainingBodyBytes()) : len;
  if (toRead == 0) {
    return size_t{0};
  }
  istream->read(reinterpret_cast<char*>(dst),
                static_cast<std::streamsize>(toRead));
  const size_t count = static_cast<size_t>(istream->gcount());
  readContentLength += count;
  if (count < toRead) {
    auto error = readFailure(Errc::IncompleteMessage);
    const bool cleanEof = istream->eof() && !istream->bad() &&
        (!socketStream || socketStream->readState() == net::ReadState::EndOfStream);
    if (sharedIstream) sharedIstream->close();
    if (count == 0 && (contentLength || !cleanEof)) {
      return nonstd::make_unexpected(error);
    }
  }
  return count;
}

bell::Result<> http::Reader::discardRemainingBody() {
  if (!headersValid) {
    return make_unexpected_errc(std::errc::operation_not_permitted);
  }
  if (!contentLength || remainingBodyBytes() > maxDrainLen) {
    return make_unexpected_errc(std::errc::message_size);
  }

  std::array<std::byte, 512> scratch;
  while (remainingBodyBytes() > 0) {
    auto res = readBodyChunk(scratch.data(), scratch.size());
    if (!res) {
      return nonstd::make_unexpected(res.error());
    }

    // Peer stopped short of Content-Length; the rest is never arriving.
    if (*res == 0) {
      return nonstd::make_unexpected(make_error_code(Errc::IncompleteMessage));
    }
  }

  return {};
}

bell::Result<> http::Reader::readBody() {
  if (!usingExternalBuffer) {
    bufferPtr = &internalBuffer;
  }

  if (!isValid(readerDirection)) {
    return make_unexpected_errc(std::errc::operation_not_permitted);
  }

  while (!contentLength || remainingBodyBytes() > 0) {
    const size_t count = contentLength ? remainingBodyBytes() : 4096;
    const size_t offset = bufferPtr->size();
    resizeBuffer(offset + count);
    auto res = readBodyChunk(reinterpret_cast<std::byte*>(bufferPtr->data() + offset), count);
    resizeBuffer(offset + (res ? *res : 0));
    if (!res) return nonstd::make_unexpected(res.error());
    if (*res == 0) break;
  }
  return {};
}

void http::Reader::resizeBuffer(size_t size) {
  const auto base = reinterpret_cast<uintptr_t>(bufferPtr->data());
  bufferPtr->resize(size);
  const auto newBase = reinterpret_cast<uintptr_t>(bufferPtr->data());
  if (newBase == base) return;
  auto relocate = [base, newBase](const char* ptr) {
    return ptr ? reinterpret_cast<const char*>(newBase +
        (reinterpret_cast<uintptr_t>(ptr) - base)) : nullptr;
  };
  for (auto& header : phrHeaders) {
    header.name = relocate(header.name);
    header.value = relocate(header.value);
  }
  if (path) path = std::string_view(relocate(path->data()), path->size());
  if (statusMessage) {
    statusMessage = std::string_view(relocate(statusMessage->data()), statusMessage->size());
  }
}

bool http::Reader::isValid(Direction expectedDirection) const {
  if (!headersValid) {
    return false;
  }

  if (readerDirection != expectedDirection) {
    return false;
  }

  return true;
}

bell::Result<int> http::Reader::getStatusCode() const {
  if (!isValid(Direction::Response)) {
    return nonstd::make_unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!statusCode.has_value()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::no_message));
  }

  return statusCode.value();
}

bell::Result<http::Method> http::Reader::getMethod() const {
  if (!isValid(Direction::Request)) {
    return nonstd::make_unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }
  if (!method.has_value()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::no_message));
  }

  return method.value();
}

bell::Result<std::string_view> http::Reader::getStatusMessage() const {
  if (!isValid(Direction::Response)) {
    return nonstd::make_unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }
  if (!statusMessage.has_value()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::no_message));
  }

  return statusMessage.value();
}

bell::Result<std::string_view> http::Reader::getPath() const {
  if (!isValid(Direction::Request)) {
    return nonstd::make_unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }
  if (!path.has_value()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::no_message));
  }

  return path.value();
}
