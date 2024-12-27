#include "bell/net/HTTPCommon.h"

#include <unordered_map>

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

void net::HTTPReader::readHeaders() {}

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
