
#pragma once

// Standard includes
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Library includes
#include "fmt/format.h"

// Own includes
#include "bell/Result.h"
#include "bell/http/Common.h"
#include "bell/http/Reader.h"
#include "bell/http/Writer.h"
#include "bell/net/SocketStream.h"
#include "bell/net/URIParser.h"

namespace bell::http {
// Default timeout for HTTP operations, in milliseconds
const int defaultHTTPClientTimeout = 5000;

class Connection {
 public:
  explicit Connection() = default;
  ~Connection() = default;

  /**
   * @brief Connects to the given URL using the specified timeout and security settings.
   *
   * @param url URL to connect to
   * @param timeoutMs Timeout in milliseconds, or 0 for no timeout
   * @param secure Whether to use HTTPS (true, port 443) or HTTP (false, port 80)
   * @return bell::Result<> Result of the connection attempt
   */
  bell::Result<> connect(const std::string& url, int timeoutMs,
                         bool secure = true) noexcept;

  /**
   * @brief Sends a HTTP request to the server, with the given method and headers.
   *
   * @param method HTTP method to use, e.g. "GET" or "POST"
   * @param extraHeaders Additional headers to include in the request
   * @param expectedContentLength Expected content length in bytes, or 0 if the request does not have a body. This is used to set the Content-Length header, if present.
   * @remark The returned Writer object will have the headers written, so the caller should only use it to write the request body.
   * @return http::Writer object, which should be used to write the body of the request.
   */
  bell::Result<Writer> sendRequest(Method method, const Headers& extraHeaders,
                                   size_t expectedContentLength);

  /**
   * @brief Returns the response object. This method should be called after sendRequest()
   * @param externalBuffer Optional pointer to an external buffer to use for reading the response. If not provided, an internal buffer will be used. Passing the external buffer will make the ownership of reader dependent on the Connection's and buffer's lifecycle.
   *
   * @return std::unique_ptr<Reader> Pointer to the response object
   */
  bell::Result<Reader> getResponse(std::vector<char>* externalBuffer = nullptr);

  // Other connection-related methods
 private:
  // Internal socket stream
  std::shared_ptr<bell::net::SocketStream> socketStream;

  // Parsed URL
  bell::net::URI parsedUrl;

  // We only send the request once
  bool requestSent = false;
};

/**
 * @brief Makes a generic request to the given URL.
 *
 * @param method HTTP method to use, e.g. "GET" or "POST"
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param timeoutMs Timeout in milliseconds
 * @param secure Whether to use HTTPS (true, port 443) or HTTP (false, port 80)
 *
 * @return std::unique_ptr<Connection> Pointer to the http::Connection. The response can be obtained by calling getResponse() on the returned object.
 */
inline bell::Result<Reader> request(Method method, const std::string& url,
                                    const Headers& headers = {},
                                    int timeoutMs = defaultHTTPClientTimeout,
                                    int secure = true) {
  Connection conn;
  auto res = conn.connect(url, timeoutMs, secure);
  if (!res) {
    return tl::unexpected(res.error());
  }

  auto writerRes = conn.sendRequest(method, headers, 0);
  if (!writerRes) {
    return tl::unexpected(writerRes.error());
  }

  return conn.getResponse();
}

/**
 * @brief Makes a generic request to the given URL.
 *
 * @param method HTTP method to use, e.g. "GET" or "POST"
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param body Body to include in the request, passed as a pointer to a byte array
 * @param length Length of the body
 * @param timeoutMs Timeout in milliseconds
 * @param secure Whether to use HTTPS (true, port 443) or HTTP (false, port 80)
 *
 * @return std::unique_ptr<Connection> Pointer to the http::Connection. The response can be obtained by calling getResponse() on the returned object.
 */
inline bell::Result<Reader> requestWithBodyPtr(
    Method method, const std::string& url, const Headers& headers = {},
    const std::byte* body = nullptr, size_t length = 0,
    int timeoutMs = defaultHTTPClientTimeout, bool secure = true) {
  Connection conn;
  auto res = conn.connect(url, timeoutMs, secure);
  if (!res) {
    return tl::unexpected(res.error());
  }
  auto writerRes = conn.sendRequest(method, headers, length);
  if (!writerRes) {
    return tl::unexpected(writerRes.error());
  }

  res = writerRes->writeBodyRaw(reinterpret_cast<const char*>(body), length);
  if (!res) {
    return tl::unexpected(res.error());
  }

  return conn.getResponse();
}

/**
 * @brief Makes a generic request to the given URL.
 *
 * @param method HTTP method to use, e.g. "GET" or "POST"
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param body Body to include in the request, passed as a vector of bytes
 * @param timeoutMs Timeout in milliseconds
 * @param secure Whether to use HTTPS (true, port 443) or HTTP (false, port 80)
 *
 * @return std::unique_ptr<Connection> Pointer to the http::Connection. The response can be obtained by calling getResponse() on the returned object.
 */
inline bell::Result<Reader> requestWithBody(
    Method method, const std::string& url, const Headers& headers = {},
    const std::vector<std::byte>& body = {},
    int timeoutMs = defaultHTTPClientTimeout, bool secure = true) {
  return requestWithBodyPtr(method, url, headers, body.data(), body.size(),
                            timeoutMs, secure);
}

}  // namespace bell::http

// Alias for the HTTPConnection class
namespace bell {
using HTTPConnection = http::Connection;
};  // namespace bell
