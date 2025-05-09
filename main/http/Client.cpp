#include "bell/http/Client.h"

// Standard includes
#include <ios>
#include <system_error>

// Own includes
#include "bell/http/Common.h"
#include "bell/http/Reader.h"
#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/net/TLSSocket.h"
#include "bell/net/URIParser.h"

using namespace bell;

bell::Result<> http::Connection::connect(const std::string& url, int timeoutMs,
                                         bool secure) noexcept {
  // Try to parse the URL
  auto parsedUri = net::parseURI(url);
  if (!parsedUri.has_value() || !parsedUri->host.has_value() ||
      !parsedUri->path.has_value()) {
    return make_error_code(http::Errc::InvalidURL);
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
      return res.getError();
    }

    // Wrap the socket in socket stream
    socketStream = std::make_shared<net::SocketStream>(std::move(socket));
  } else {
    auto socket = std::make_shared<net::TCPSocket>();
    auto res = socket->connect(parsedUri->host.value(),
                               parsedUri->port.value_or(80), timeoutMs);

    if (!res) {
      return res.getError();
    }

    // Wrap the socket in a socket stream
    socketStream = std::make_shared<net::SocketStream>(std::move(socket));
  }

  return {};
}

bell::Result<http::Writer> http::Connection::sendRequest(
    Method method, const Headers& extraHeaders, size_t expectedContentLength) {

  if (requestSent) {
    return make_error_code(http::Errc::InvalidState);
  }

  if (!socketStream->isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Create the writer
  http::Writer writer(http::Direction::Request, socketStream);

  // Set the host header
  writer.setHeader("Host", parsedUrl.host.value());

  std::string requestPath = parsedUrl.path.value();

  if (parsedUrl.query.has_value()) {
    requestPath += "?" + parsedUrl.query.value();
  }

  // Write the actual request
  writer.writeRequest(method, requestPath, extraHeaders, expectedContentLength);
  requestSent = true;

  socketStream->flush();
  return writer;
}

bell::Result<http::Reader> http::Connection::getResponse() {
  if (!requestSent) {
    return make_error_code(http::Errc::InvalidState);
  }

  if (!socketStream->isOpen()) {
    return make_error_code(http::Errc::SocketNotOpen);
  }

  http::Reader reader = http::Reader(http::Direction::Response, socketStream);
  // Read the response headers
  reader.readHeaders();

  return reader;
}
