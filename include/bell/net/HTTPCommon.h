#pragma once

#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "picohttpparser.h"

namespace bell::net {

// Type definition for HTTP headers
using Header = std::pair<std::string, std::string>;

// Type definition for a list of HTTP headers
using Headers = std::vector<Header>;

enum class HTTPType { Request, Response };

enum class HTTPMethod { GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH, INVALID };

HTTPMethod parseHTTPMethod(std::string_view method);

class HTTPReader {
 public:
  HTTPReader(HTTPType readerType, std::istream* istream,
             std::vector<char>* externalBuffer = nullptr);

  void readHeaders();

  std::string_view getHeader(const std::string& headerName) const;

  Headers getAllHeaders() const;

  int getStatusCode() const;

  std::string_view getPath() const;

  HTTPMethod getMethod() const;

  void invalidate();

 private:
  HTTPType readerType;
  std::istream* istream;
  std::vector<char> internalBuffer;
  std::vector<char>* bufferPtr;

  bool headersValid = false;

  // picohttpparser headers
  std::vector<phr_header> phrHeaders;

  std::optional<size_t> contentLength;

  // Request specific fields
  std::optional<HTTPMethod> method;
  std::optional<std::string_view> path;

  // Response specific fields
  std::optional<int> statusCode;
  std::optional<std::string_view> statusMessage;

  void ensureValid(HTTPType expectedType) const;
};
}  // namespace bell::net
