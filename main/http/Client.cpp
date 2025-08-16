#include "bell/http/Client.h"
#include "bell/Result.h"
#include "bell/http/Common.h"
#include "bell/http/Writer.h"
#include "bell/net/SocketStream.h"
#include "bell/net/TLSSocket.h"
#include "bell/net/URIParser.h"
#include "tl/expected.hpp"

using namespace bell::http;

bell::Result<Response> DefaultTransport::execute(const Request& req) {
  std::shared_ptr<net::SocketStream> socketStream;

  if (req.uri.scheme == "https") {
    auto socket = std::make_shared<net::TLSSocket>();
    auto res = socket->connect(*req.uri.host, req.uri.port.value_or(443),
                               req.operationTimeoutMs.value_or(0));
    if (!res) {
      return tl::make_unexpected(res.error());
    }

    socketStream = std::make_shared<net::SocketStream>(std::move(socket));
  } else {
    auto socket = std::make_shared<net::TCPSocket>();
    auto res = socket->connect(*req.uri.host, req.uri.port.value_or(443),
                               req.operationTimeoutMs.value_or(0));
    if (!res) {
      return tl::make_unexpected(res.error());
    }

    socketStream = std::make_shared<net::SocketStream>(std::move(socket));
  }

  // Create a writer for the request
  http::Writer writer(Direction::Request, socketStream);
  // Set the host header
  writer.setHeader("Host", *req.uri.host);

  std::string requestPath = *req.uri.path;
  // Handle query parameters if present
  if (req.uri.query.has_value()) {
    requestPath += "?" + *req.uri.query;
  }

  // Write the request
  auto res = writer.writeRequest(req.method, requestPath, req.headers,
                                 req.contentLength.value_or(0));

  if (!res) {
    return tl::make_unexpected(res.error());
  }

  // Process a body, if its present
  if (req.contentLength.value_or(0) > 0) {
    // Get the underlying output stream from your writer
    std::ostream& outStream = *writer.getStream();

    std::visit(
        [&outStream](auto&& bodyContent) {
          using T = std::decay_t<decltype(bodyContent)>;

          // Case 1: The body is a pre-buffered vector of bytes
          if constexpr (std::is_same_v<T, std::vector<std::byte>> ||
                        std::is_same_v<T, std::string_view> ||
                        std::is_same_v<T, tcb::span<std::byte>>) {
            if (!bodyContent.empty()) {
              outStream.write(reinterpret_cast<const char*>(bodyContent.data()),
                              bodyContent.size());
            }
          }
          // Case 2: The body is a stream (the zero-copy path)
          else if constexpr (std::is_same_v<T, std::istream*>) {
            if (bodyContent) {  // Check for non-null pointer
              std::istream& inStream = *bodyContent;
              std::array<char, 1024> buffer{};

              while (!inStream.eof()) {
                inStream.read(buffer.data(), sizeof(buffer));
                std::streamsize bytesRead = inStream.gcount();
                if (bytesRead > 0) {
                  outStream.write(buffer.data(), bytesRead);
                }
              }
            }
          }
          // Case 3 (std::monostate): Do nothing, there is no body.
        },
        req.body);
  }

  socketStream->flush();

  if (socketStream->bad()) {
    return bell::make_unexpected_errc<Response>(std::errc::io_error);
  }

  // Create a reader for the response
  http::Reader reader(Direction::Response, socketStream);

  // Try to read the headers
  res = reader.readHeaders();
  if (!res) {
    return tl::make_unexpected(res.error());
  }

  // Move the reader into the response
  return {std::move(reader)};
}

bell::Result<Request> Request::create(http::Method method,
                                      const std::string& url) {
  auto parsedUrl = bell::net::parseURI(url);
  if (!parsedUrl) {
    return tl::make_unexpected(http::Errc::InvalidURL);
  }
  return Request({method, *parsedUrl});
}

Response::Response(http::Reader responseReader)
    : bodyReader(std::move(responseReader)) {
  // Extract status code and message from the reader
  statusCode = *bodyReader.getStatusCode();
  headers = bodyReader.getAllHeaders();
  contentLength = bodyReader.getContentLength();
  statusMessage = *bodyReader.getStatusMessage();
}

bell::Result<std::string_view> Response::text() {
  return bodyReader.getBodyStringView();
}

bell::Result<std::vector<std::byte>> Response::bytes() {
  return bodyReader.getBodyBytes();
}

bell::Result<const std::byte*> Response::bytesPtr() {
  return bodyReader.getBodyBytesPtr();
}

std::istream* Response::stream() const {
  return bodyReader.getStream();
}

bell::Result<Response> Client::rawRequest(Request& req) {
  std::visit(
      [&req](auto&& bodyContent) {
        using T = std::decay_t<decltype(bodyContent)>;

        if constexpr (std::is_same_v<T, std::vector<std::byte>> ||
                      std::is_same_v<T, std::string_view> ||
                      std::is_same_v<T, tcb::span<std::byte>>) {
          req.contentLength = bodyContent.size();

        } else if constexpr (std::is_same_v<T, std::monostate>) {
          // No body, no content length
          req.contentLength = 0;
        }
      },
      req.body);

  return transport->execute(req);
}

bell::Result<Response> Client::get(const std::string& url,
                                   const Headers& headers) {
  auto req = Request::create(Method::GET, url);
  if (!req) {
    return tl::make_unexpected(req.error());
  }
  req->headers = headers;
  req->operationTimeoutMs = operationTimeoutMs;
  return rawRequest(*req);
}

bell::Result<Response> Client::post(const std::string& url,
                                    const Headers& headers, RequestBody body,
                                    std::optional<size_t> bodyLength) {
  auto req = Request::create(Method::POST, url);
  if (!req) {
    return tl::make_unexpected(req.error());
  }
  req->headers = headers;
  req->operationTimeoutMs = operationTimeoutMs;
  req->body = std::move(body);
  req->contentLength = bodyLength;

  return rawRequest(*req);
}

bell::Result<Response> Client::put(const std::string& url,
                                   const Headers& headers, RequestBody body,
                                   std::optional<size_t> bodyLength) {
  auto req = Request::create(Method::PUT, url);
  if (!req) {
    return tl::make_unexpected(req.error());
  }
  req->headers = headers;
  req->operationTimeoutMs = operationTimeoutMs;
  req->body = std::move(body);
  req->contentLength = bodyLength;

  return rawRequest(*req);
}
