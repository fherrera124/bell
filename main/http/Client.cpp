#include "bell/http/Client.h"

// Standar includes
#include <ios>

// Own includes
#include "bell/http/Reader.h"
#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/net/URIParser.h"

using namespace bell;

http::Connection::Connection(const std::string& url, int timeoutMs) {
  // Try to parse the URL
  auto parsedUri = net::parseURI(url);
  if (!parsedUri.has_value() || !parsedUri->host.has_value() ||
      !parsedUri->path.has_value()) {
    throw std::runtime_error("Invalid URL");
  }

  // Store the parsed URL
  parsedUrl = parsedUri.value();

  // Create a socket to the host
  // TODO: Add TLS support here
  auto socket = std::make_shared<net::TCPSocket>();
  socket->connect(parsedUri->host.value(), parsedUri->port.value_or(80),
                  timeoutMs);

  // Wrap the socket in a socket stream
  socketStream =
      std::make_shared<net::SocketStream>(std::move(socket), timeoutMs);
}

std::unique_ptr<http::Writer> http::Connection::sendRequest(
    Method method, const Headers& extraHeaders, size_t expectedContentLength) {

  if (requestSent) {
    throw std::runtime_error("Request already sent");
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Create the writer
  auto writer = std::make_unique<http::Writer>(http::Direction::Request,
                                               socketStream.get());
  // Set the host header
  writer->setHeader("host", parsedUrl.host.value());

  std::string requestPath = parsedUrl.path.value();

  if (parsedUrl.query.has_value()) {
    requestPath += "?" + parsedUrl.query.value();
  }

  // Write the actual request
  writer->writeRequest(method, requestPath, extraHeaders,
                       expectedContentLength);
  requestSent = true;

  socketStream->flush();
  return writer;
}

std::unique_ptr<http::Reader> http::Connection::getResponse() {
  if (!requestSent) {
    throw std::runtime_error("Request not sent yet");
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  auto reader = std::make_unique<http::Reader>(http::Direction::Response,
                                               socketStream.get());
  // Read the response headers
  reader->readHeaders();

  return reader;
}
