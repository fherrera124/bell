#include "bell/http/Client.h"

// Standard includes
#include <system_error>

// Own includes
#include "bell/http/Common.h"
#include "bell/http/Reader.h"
#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/net/TLSSocket.h"
#include "bell/net/URIParser.h"
#include "tl/expected.hpp"

using namespace bell;

bell::Result<> http::Connection::connect(const std::string& url, int timeoutMs,
                                         bool secure) noexcept {
  // Try to parse the URL
  auto parsedUri = net::parseURI(url);
  if (!parsedUri.has_value() || !parsedUri->host.has_value() ||
      !parsedUri->path.has_value()) {
    return tl::make_unexpected(http::Errc::InvalidURL);
  }

  // Store the parsed URL
  parsedUrl = parsedUri.value();

  // Create a socket to the host
  bool useTLS = secure && parsedUri->scheme == "https";

  if (useTLS) {
    auto socket = std::make_shared<net::TLSSocket>();
    auto res = socket->connect(parsedUri->host.value(),
                               parsedUri->port.value_or(443), timeoutMs);

    if (!res) {
      return res;
    }

    // Wrap the socket in socket stream
    socketStream = std::make_shared<net::SocketStream>(std::move(socket));
  } else {
    auto socket = std::make_shared<net::TCPSocket>();
    auto res = socket->connect(parsedUri->host.value(),
                               parsedUri->port.value_or(80), timeoutMs);

    if (!res) {
      return res;
    }

    // Wrap the socket in a socket stream
    socketStream = std::make_shared<net::SocketStream>(std::move(socket));
  }

  return {};
}

tl::expected<http::Writer, std::error_code> http::Connection::sendRequest(
    Method method, const Headers& extraHeaders, size_t expectedContentLength) {
  if (requestSent) {
    return tl::make_unexpected(http::Errc::InvalidState);
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Create the writer
  auto writer = http::Writer(http::Direction::Request, socketStream);

  // Set the host header
  writer.setHeader("Host", parsedUrl.host.value());

  std::string requestPath = parsedUrl.path.value();

  if (parsedUrl.query.has_value()) {
    requestPath += "?" + parsedUrl.query.value();
  }

  // Write the actual request
  auto res = writer.writeRequest(method, requestPath, extraHeaders,
                                 expectedContentLength);
  if (!res) {
    return tl::make_unexpected(res.error());
  }
  requestSent = true;

  socketStream->flush();
  return writer;
}

bell::Result<http::Reader> http::Connection::getResponse(
    std::vector<char>* externalBuffer) {
  if (!requestSent) {
    return tl::make_unexpected(http::Errc::InvalidState);
  }

  if (!socketStream->isOpen()) {
    return tl::make_unexpected(http::Errc::SocketNotOpen);
  }

  auto reader = externalBuffer
                    ? http::Reader(http::Direction::Response, socketStream.get(),
                                   externalBuffer)
                    : http::Reader(http::Direction::Response, socketStream);

  // Read the response headers
  auto res = reader.readHeaders();
  if (!res) {
    return tl::make_unexpected(res.error());
  }

  // Reset the request state
  requestSent = false;

  return reader;
}
