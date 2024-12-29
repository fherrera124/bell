#include "bell/http/Writer.h"

#include <algorithm>
#include <ios>
#include <unordered_map>

using namespace bell;
namespace {
// Map of status codes to their corresponding status messages
const std::unordered_map<int, std::string> statusCodes = {
    {200, "OK"},
    {404, "Not Found"},
    {500, "Internal Server Error"},
};
}  // namespace

http::Writer::Writer(Direction writerDirection, std::ostream* ostream)
    : writerDirection(writerDirection), ostream(ostream) {}

std::string http::Writer::getStatusMessage() {
  if (!statusCode) {
    throw std::runtime_error("Status code not set");
  }

  auto it = statusCodes.find(*statusCode);
  if (it == statusCodes.end()) {
    return "Unknown";
  }
  return it->second;
}

void http::Writer::writeHeaders() {
  if (headersWritten) {
    throw std::runtime_error("Headers already written");
  }

  // Will fill in standard headers if they are not already set
  enforceStandardHeaders();

  if (writerDirection == Direction::Request) {
    if (method && path) {
      *ostream << methodToString(*method) << " " << *path << " HTTP/1.1\r\n";
    } else {
      throw std::runtime_error("Method and path must be set for requests");
    }
  } else {
    if (statusCode) {
      *ostream << "HTTP/1.1 " << *statusCode << " " << getStatusMessage()
               << "\r\n";
    } else {
      throw std::runtime_error("Status code must be set for responses");
    }
  }

  for (const auto& header : headers) {
    *ostream << header.first << ": " << header.second << "\r\n";
  }

  if (contentLength > 0) {
    *ostream << "content-length: " << contentLength << "\r\n";
  }

  *ostream << "\r\n";  // End of headers
  ostream->flush();    // Flush the headers to the stream

  headersWritten = true;
}

void http::Writer::setHeader(const std::string& headerName,
                             const std::string& headerValue) {
  headers[headerName] = headerValue;
}

void http::Writer::setHeaders(const Headers& headers) {
  for (const auto& header : headers) {
    setHeader(header.first, header.second);
  }
}

void http::Writer::enforceStandardHeaders() {
  if (writerDirection == Direction::Request) {
    if (headers.find("host") == headers.end()) {
      headers["host"] =
          "localhost";  // Default host, should be overridden by the actual host
    }
    if (headers.find("user-agent") == headers.end()) {
      headers["user-agent"] = defaultUserAgent;
    }
  } else {
    // Standard response headers
    if (contentLength > 0 && headers.find("content-type") == headers.end()) {
      headers["content-type"] = "text/html";  // Default content type
    }
  }
}

void http::Writer::setContentLength(size_t contentLength) {
  ensureValid(writerDirection);

  this->contentLength = contentLength;
}

void http::Writer::writeRequest(Method method, const std::string& path,
                                const Headers& headers,
                                size_t expectedContentLength) {
  ensureValid(Direction::Request);

  // Assign the request parameters
  setMethod(method);
  setPath(path);
  setHeaders(headers);
  setContentLength(expectedContentLength);

  writeHeaders();
}

void http::Writer::writeResponse(int statusCode, const Headers& headers,
                                 size_t expectedContentLength) {
  ensureValid(Direction::Response);

  // Assign the request parameters
  setStatusCode(statusCode);
  setHeaders(headers);
  setContentLength(expectedContentLength);

  writeHeaders();
}

void http::Writer::writeResponseWithBody(int statusCode, const Headers& headers,
                                         const std::string& body) {
  writeResponse(statusCode, headers, body.size());
  writeBodyStringView(body);
}

bool http::Writer::hasWrittenHeaders() const {
  return headersWritten;
}

bool http::Writer::hasWrittenBody() const {
  return contentLengthWritten >= contentLength;
}

void http::Writer::ensureValid(Direction expectedDirection) {
  if (headersWritten) {
    throw std::runtime_error("HTTP headers have already been written");
  }

  if (writerDirection != expectedDirection) {
    throw std::runtime_error("This method is not valid for this writer type");
  }
}

void http::Writer::setPath(const std::string& path) {
  ensureValid(Direction::Request);

  this->path = path;
}

void http::Writer::setMethod(Method method) {
  ensureValid(Direction::Request);

  this->method = method;
}

void http::Writer::setStatusCode(int statusCode) {
  ensureValid(Direction::Response);

  this->statusCode = statusCode;
}

std::ostream* http::Writer::getStream() const {
  return ostream;
}

void http::Writer::writeBodyRaw(const char* bytes, size_t bytesLen) {
  if (!headersWritten) {
    throw std::runtime_error("Headers must be written before writing body");
  }

  if (contentLengthWritten + bytesLen > contentLength) {
    throw std::runtime_error(
        "Body length exceeds previously declared content length");
  }

  ostream->write(bytes, static_cast<std::streamsize>(bytesLen));
  contentLengthWritten += bytesLen;

  ostream->flush();  // Flush the headers to the stream
}

void http::Writer::writeBodyStringView(std::string_view body) {
  writeBodyRaw(body.data(), body.size());
}
