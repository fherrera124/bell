
#pragma once

// System includes
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Library includes
#include "bell/http/Writer.h"
#include "fmt/format.h"

// Own includes
#include "bell/http/Reader.h"
#include "bell/net/SocketStream.h"
#include "bell/net/URIParser.h"

namespace bell::http {
// Default timeout for HTTP operations, in milliseconds
const int defaultHTTPClientTimeout = 5000;

class Connection {
 public:
  explicit Connection(const std::string& url,
                      int timeoutMs = defaultHTTPClientTimeout);
  ~Connection() = default;

  /**
   * @brief Sends a HTTP request to the server, with the given method and headers.
   *
   * @param method HTTP method to use, e.g. "GET" or "POST"
   * @param extraHeaders Additional headers to include in the request
   * @param expectedContentLength Expected content length in bytes, or 0 if the request does not have a body. This is used to set the Content-Length header, if present.
   * @remark The returned Writer object will have the headers written, so the caller should only use it to write the request body.
   * @return std::unique_ptr<Writer> Pointer to the http::Writer object, which should be used to write the body of the request.
   */
  std::unique_ptr<Writer> sendRequest(Method method,
                                      const Headers& extraHeaders,
                                      size_t expectedContentLength);

  /**
   * @brief Returns the response object. This method should be called after sendRequest()
   *
   * @return std::unique_ptr<Reader> Pointer to the response object
   */
  std::unique_ptr<Reader> getResponse();

  // Other connection-related methods
 private:
  // Internal socket stream
  std::unique_ptr<bell::net::SocketStream> socketStream;

  // Parsed URL
  bell::net::URI parsedUrl;

  // We only send the request once
  bool requestSent = false;
};

/**
 * @brief Makes a GET request to the given URL.
 *
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param timeoutMs Timeout in milliseconds
 *
 * @return std::unique_ptr<Connection> Pointer to the http::Connection. The response can be obtained by calling getResponse() on the returned object.
 */
inline std::unique_ptr<Connection> get(const std::string& url,
                                       const Headers& headers = {},
                                       int timeoutMs = defaultHTTPClientTimeout) {
  auto connection = std::make_unique<Connection>(url, timeoutMs);
  connection->sendRequest(Method::GET, headers, 0);
  return connection;
}

/**
 * @brief Makes a POST request to the given URL.
 *
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param body Body to include in the request, passed as a pointer to a byte array
 * @param length Length of the body
 * @param timeoutMs Timeout in milliseconds
 *
 * @return std::unique_ptr<Connection> Pointer to the http::Connection. The response can be obtained by calling getResponse() on the returned object.
 */
inline std::unique_ptr<Connection> postRawPtr(
    const std::string& url, const Headers& headers = {},
    const std::byte* body = nullptr, size_t length = 0,
    int timeoutMs = defaultHTTPClientTimeout) {
  auto connection = std::make_unique<Connection>(url, timeoutMs);
  auto reader = connection->sendRequest(Method::POST, headers, length);
  reader->writeBodyRaw(reinterpret_cast<const char*>(body), length);
  return connection;
}

/**
 * @brief Makes a GET request to the given URL.
 *
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param body Body to include in the request, passed as a vector of bytes
 * @param timeoutMs Timeout in milliseconds
 *
 * @return std::unique_ptr<Connection> Pointer to the http::Connection. The response can be obtained by calling getResponse() on the returned object.
 */
inline std::unique_ptr<Connection> post(const std::string& url,
                                        const Headers& headers = {},
                                        const std::vector<std::byte>& body = {},
                                        int timeoutMs = defaultHTTPClientTimeout) {
  return postRawPtr(url, headers, body.data(), body.size(), timeoutMs);
}
}  // namespace bell::http
