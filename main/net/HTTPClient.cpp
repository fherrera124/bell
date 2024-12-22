#include "bell/net/HTTPClient.h"

// System includes
#include <ios>

// Library includes
#include "picohttpparser.h"

// Own includes
#include "bell/net/TCPSocket.h"
#include "bell/net/URIParser.h"

using namespace bell::net;
namespace {
// HTTP newline
const char* requestNewLine = "\r\n";
}  // namespace

void HTTPResponse::readHeaders() {
  if (!isValid || socketStream == nullptr) {
    throw std::runtime_error("Response is not valid");
  }

  if (!socketStream->isOpen()) {
    isValid = false;
    throw std::runtime_error("Socket is not open");
  }

  const char* msgPointer = nullptr;
  size_t msgLen = 0;
  int minorVersion = 0;

  std::string line;  // Use this to read lines from the stream
  size_t numHeaders = 0;

  while (std::getline(*socketStream, line) && !line.empty()) {
    // Restore the newline character
    line.append("\n");

    responseBuffer.insert(responseBuffer.end(), line.begin(), line.end());

    // Reserve space for the headers
    phResponseHeaders.push_back({});
    numHeaders = phResponseHeaders.size();

    int ret = phr_parse_response(responseBuffer.data(), responseBuffer.size(),
                                 &minorVersion, &statusCode, &msgPointer,
                                 &msgLen, phResponseHeaders.data(), &numHeaders,
                                 responseBuffer.size() - line.size());

    if (ret > 0) {
      break;  // Successfully parsed the headers, done reading
    }

    if (ret == -1) {
      throw std::runtime_error("Failed to parse HTTP response headers");
    }
  }

  auto contentLengthHeader = getHeader("Content-Length");
  if (!contentLengthHeader.empty()) {
    contentLength = std::stoi(std::string(contentLengthHeader));
  }
}

void HTTPResponse::readBody() {
  if (!isValid || socketStream == nullptr) {
    throw std::runtime_error("Response is not valid");
  }

  if (!socketStream->isOpen()) {
    isValid = false;
    throw std::runtime_error("Socket is not open");
  }

  if (contentLength == 0 || readContentLength == contentLength) {
    return;  // Nothing to read
  }

  // Ensure that the response buffer has enough space to read the content
  responseBuffer.resize(responseBuffer.size() + contentLength -
                        readContentLength);

  // Read the content
  socketStream->read(
      responseBuffer.data() + responseBuffer.size() - contentLength,
      static_cast<std::streamsize>(contentLength - readContentLength));

  // Update the read content length
  readContentLength += socketStream->gcount();
}

size_t HTTPResponse::getContentLength() const {
  return contentLength;
}

int HTTPResponse::getStatusCode() const {
  return statusCode;
}

HTTPResponse::~HTTPResponse() {
  if (socketStream != nullptr) {
    socketStream->close();
  }
}

std::unique_ptr<SocketStream> HTTPResponse::getStream() {
  isValid = false;  // Invalidate the response object
  return std::move(socketStream);
}

std::string_view HTTPResponse::getBodyStringView() {
  if (readContentLength == 0) {
    readBody();
  }

  return {responseBuffer.data() + responseBuffer.size() - readContentLength,
          readContentLength};
}

std::vector<std::byte> HTTPResponse::getBodyBytes() {
  if (readContentLength == 0) {
    readBody();
  }

  return {
      reinterpret_cast<std::byte*>(responseBuffer.data() +
                                   responseBuffer.size() - readContentLength),
      reinterpret_cast<std::byte*>(responseBuffer.data() +
                                   responseBuffer.size()),
  };
}

const char* HTTPResponse::getBodyBytesPtr() {
  if (readContentLength == 0) {
    readBody();
  }

  return responseBuffer.data() + responseBuffer.size() - readContentLength;
}

size_t HTTPResponse::getBodyBytesLength() {
  if (readContentLength == 0) {
    readBody();
  }

  return readContentLength;
}

std::string_view HTTPResponse::getHeader(const std::string& headerName) const {
  for (const auto& header : phResponseHeaders) {
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

HTTPHeaders HTTPResponse::getAllHeaders() const {
  HTTPHeaders headers;
  for (const auto& header : phResponseHeaders) {
    headers.emplace_back(std::string(header.name, header.name_len),
                         std::string(header.value, header.value_len));
  }
  return headers;
}

HTTPValueHeader httpRangeHeader(std::optional<int32_t> start,
                                std::optional<int32_t> end) {
  std::string header = "bytes=";
  if (start.has_value()) {
    header += std::to_string(start.value());
  }
  header += "-";
  if (end.has_value()) {
    header += std::to_string(end.value());
  }
  return std::make_pair("Range", header);
}

HTTPConnection::~HTTPConnection() {
  if (socketStream != nullptr) {
    socketStream->close();
  }
}

HTTPConnection::HTTPConnection(const std::string& url, int timeoutMs) {
  // Try to parse the URL
  auto parsedUri = parseURI(url);
  if (!parsedUri.has_value()) {
    throw std::runtime_error("Invalid URL");
  }

  // Store the parsed URL
  parsedUrl = parsedUri.value();

  // Create a socket to the host
  // TODO: Add TLS support here
  auto socket = std::make_unique<TCPSocket>();
  socket->connect(parsedUri->host.value(), parsedUri->port.value_or(80),
                  timeoutMs);

  // Wrap the socket in a socket stream
  socketStream = std::make_unique<SocketStream>(std::move(socket), timeoutMs);
}

void HTTPConnection::sendRequest(const std::string& method,
                                 const HTTPHeaders& extraHeaders,
                                 size_t expectedContentLength) {

  if (requestSent) {
    throw std::runtime_error("Request already sent");
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  requestSent = true;
  this->expectedContentLength = expectedContentLength;

  // Send the request
  *socketStream << method << " " << parsedUrl.path.value_or("/") << " HTTP/1.1"
                << requestNewLine;
  *socketStream << "Host: " << parsedUrl.host.value();
  // Only add the port if it is not the default port
  if (parsedUrl.port.has_value() && parsedUrl.port.value() != 80 &&
      parsedUrl.port.value() != 443) {
    *socketStream << ":" << parsedUrl.port.value();
  }

  *socketStream << requestNewLine;

  // Send the default headers
  *socketStream << "Connection: keep-alive" << requestNewLine;
  *socketStream << "User-Agent: " << userAgent << requestNewLine;
  *socketStream << "Accept: */*" << requestNewLine;

  if (expectedContentLength > 0) {
    *socketStream << "Content-Length: " << expectedContentLength
                  << requestNewLine;
  }

  // Write extra headers
  for (const auto& header : extraHeaders) {
    *socketStream << header.first << ": " << header.second << requestNewLine;
  }

  // End of headers
  *socketStream << requestNewLine;

  // Flush the stream
  socketStream->flush();

  // The request is now sent. If the request has a body, it should immediately be sent after this.
}

void HTTPConnection::sendFullBody(const std::byte* body, size_t length) {
  if (!requestSent) {
    throw std::runtime_error("Request not sent yet");
  }

  if (expectedContentLength != length) {
    throw std::runtime_error("Content length mismatch");
  }

  // Write the body
  socketStream->write(reinterpret_cast<const char*>(body), length);

  // Flush the stream
  socketStream->flush();
}

std::unique_ptr<HTTPResponse> HTTPConnection::getResponse() {
  if (!requestSent) {
    throw std::runtime_error("Request not sent yet");
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Read the response
  return std::make_unique<HTTPResponse>(std::move(socketStream));
}
