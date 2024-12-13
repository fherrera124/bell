#include "bell/io/HTTPClient.h"
#include "bell/io/TCPSocket.h"
#include "bell/io/URIParser.h"

using namespace bell;

namespace {
// HTTP newline
const char* requestNewLine = "\r\n";
}  // namespace

http::Response::~Response() {
  if (socketStream != nullptr) {
    socketStream->close();
  }
}
http::ValueHeader http::rangeHeader(std::optional<int32_t> start,
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

http::Connection::~Connection() {
  if (socketStream != nullptr) {
    socketStream->close();
  }
}

http::Connection::Connection(const std::string& url, int timeoutMs) {
  // Try to parse the URL
  auto parsedUri = uri::parse(url);
  if (!parsedUri.has_value()) {
    throw std::runtime_error("Invalid URL");
  }

  // Store the parsed URL
  parsedUrl = parsedUri.value();

  // Create a socket to the host
  // TODO: Add TLS support here
  auto socket = std::make_unique<io::TCPSocket>();
  socket->connect(parsedUri->host.value(), parsedUri->port.value_or(80),
                  timeoutMs);

  // Wrap the socket in a socket stream
  socketStream =
      std::make_unique<io::SocketStream>(std::move(socket), timeoutMs);
}

void http::Connection::sendRequest(const std::string& method,
                                   const Headers& extraHeaders,
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

void http::Connection::sendFullBody(const std::byte* body, size_t length) {
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

std::unique_ptr<http::Response> http::Connection::getResponse() {
  if (!requestSent) {
    throw std::runtime_error("Request not sent yet");
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Read the response
  return std::make_unique<Response>(std::move(socketStream));
}