#include "bell/http/Reader.h"
#include "bell/http/Common.h"

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
  }

  headersValid = false;
}

void http::Reader::readHeaders() {
  if (headersValid) {
    throw std::runtime_error("Headers already read");
  }

  int minorVersion = 0;
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

  // Consume the stream byte by byte, so we dont read into the body
  while (lastPhrResult <= 0 && istream->get(lastChar)) {
    if (bufferPtr->size() > maxRequestLen) {
      throw std::runtime_error("Request too large");
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
        throw std::runtime_error("Error parsing HTTP headers");
      }

      lastLineStart = bufferPtr->size();
    }
  }

  if (lastPhrResult <= 0) {
    throw std::runtime_error(
        "Could not read all headers, possibly due to a broken connection");
  }

  headersValid = true;  // Mark headers as read

  // Assign the content length
  auto contentLengthHeader = getHeader("Content-Length");
  if (!contentLengthHeader.empty()) {
    contentLength = std::stoi(std::string(contentLengthHeader));
  }

  if (readerDirection == Direction::Response) {
    statusCode = parsedStatusCode;
    statusMessage = std::string(statusMessagePtr, statusMessageLen);

    if (statusCode < 100 || statusCode >= 600) {
      throw std::runtime_error("Invalid status code");
    }
  } else {
    path = std::string_view(pathPtr, pathLen);
    method = parseMethod({methodPtr, methodLen});

    if (minorVersion == 1 && getHeader("Host").empty()) {
      throw std::runtime_error("Host header required for HTTP/1.1");
    }
  }
}

size_t http::Reader::getContentLength() const {
  return contentLength.value();
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
    headers.emplace_back(std::string(header.name, header.name_len),
                         std::string(header.value, header.value_len));
  }
  return headers;
}

std::string_view http::Reader::getBodyStringView() {
  if (readContentLength == 0) {
    readBody();
  }

  return {bufferPtr->data() + bufferPtr->size() - readContentLength,
          readContentLength};
}

std::vector<std::byte> http::Reader::getBodyBytes() {
  if (readContentLength == 0) {
    readBody();
  }

  return {
      reinterpret_cast<std::byte*>(bufferPtr->data() +
                                   bufferPtr->size() - readContentLength),
      reinterpret_cast<std::byte*>(bufferPtr->data() +
                                   bufferPtr->size()),
  };
}

const char* http::Reader::getBodyBytesPtr() {
  if (readContentLength == 0) {
    readBody();
  }

  return bufferPtr->data() + bufferPtr->size() - readContentLength;
}

size_t http::Reader::getBodyBytesLength() {
  if (readContentLength == 0) {
    readBody();
  }

  return readContentLength;
}


void http::Reader::readBody() {
  ensureValid(readerDirection);

  if (contentLength == 0 || readContentLength == contentLength) {
    return;  // Nothing to read
  }

  // Ensure that the response buffer has enough space to read the content
  bufferPtr->resize(bufferPtr->size() + contentLength.value() -
                    readContentLength);

  // Read the content
  istream->read(
      bufferPtr->data() + bufferPtr->size() - contentLength.value(),
      static_cast<std::streamsize>(contentLength.value() - readContentLength));

  // Update the read content length
  readContentLength += istream->gcount();
}

void http::Reader::ensureValid(Direction expectedDirection) const {
  if (!headersValid) {
    throw std::runtime_error("HTTP headers not read yet. Call readHeaders()");
  }

  if (readerDirection != expectedDirection) {
    throw std::runtime_error("This method is not valid for this reader type");
  }
}

int http::Reader::getStatusCode() const {
  ensureValid(Direction::Response);

  return statusCode.value();
}

http::Method http::Reader::getMethod() const {
  ensureValid(Direction::Request);

  return method.value();
}

std::string_view http::Reader::getPath() const {
  ensureValid(Direction::Request);

  return path.value();
}
