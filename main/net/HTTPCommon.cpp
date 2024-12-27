#include "bell/net/HTTPCommon.h"

#include <iostream>
#include <unordered_map>
#include "picohttpparser.h"

using namespace bell;

namespace {
const std::unordered_map<std::string_view, net::HTTPMethod> strMethodMap = {
    {"GET", net::HTTPMethod::GET},   {"POST", net::HTTPMethod::POST},
    {"PUT", net::HTTPMethod::PUT},   {"DELETE", net::HTTPMethod::DELETE},
    {"HEAD", net::HTTPMethod::HEAD}, {"OPTIONS", net::HTTPMethod::OPTIONS},
};
}

net::HTTPMethod net::parseHTTPMethod(std::string_view method) {
  auto it = strMethodMap.find(method);
  if (it != strMethodMap.end()) {
    return it->second;
  }

  return net::HTTPMethod::INVALID;
}

net::HTTPReader::HTTPReader(HTTPType readerType, std::istream* istream,
                            std::vector<char>* externalBuffer)
    : readerType(readerType), istream(istream), bufferPtr(externalBuffer) {
  if (bufferPtr == nullptr) {
    // External buffer not provided, use internal buffer
    bufferPtr = &internalBuffer;
  } else {
    bufferPtr->clear();
  }

  headersValid = false;
}

void net::HTTPReader::readHeaders() {
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

      if (readerType == HTTPType::Request) {
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

  if (readerType == HTTPType::Response) {
    statusCode = parsedStatusCode;
    statusMessage = std::string(statusMessagePtr, statusMessageLen);

    if (statusCode < 100 || statusCode >= 600) {
      throw std::runtime_error("Invalid status code");
    }
  } else {
    path = std::string_view(pathPtr, pathLen);
    method = net::parseHTTPMethod({methodPtr, methodLen});

    if (minorVersion == 1 && getHeader("Host").empty()) {
      throw std::runtime_error("Host header required for HTTP/1.1");
    }
  }
}

size_t net::HTTPReader::getContentLength() const {
  return contentLength.value();
}

std::string_view net::HTTPReader::getHeader(
    const std::string& headerName) const {
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

net::Headers net::HTTPReader::getAllHeaders() const {
  net::Headers headers{};
  for (const auto& header : phrHeaders) {
    headers.emplace_back(std::string(header.name, header.name_len),
                         std::string(header.value, header.value_len));
  }
  return headers;
}

void net::HTTPReader::ensureValid(net::HTTPType expectedType) const {
  if (!headersValid) {
    throw std::runtime_error("HTTP headers not read yet. Call readHeaders()");
  }

  if (readerType != expectedType) {
    throw std::runtime_error("This method is not valid for this reader type");
  }
}

int net::HTTPReader::getStatusCode() const {
  ensureValid(HTTPType::Response);

  return statusCode.value();
}

net::HTTPMethod net::HTTPReader::getMethod() const {
  ensureValid(HTTPType::Request);

  return method.value();
}

std::string_view net::HTTPReader::getPath() const {
  ensureValid(HTTPType::Request);

  return path.value();
}
