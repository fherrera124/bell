
#pragma once

#include <cstddef>  // for size_t
#include <cstdint>  // for uint8_t, int32_t
#include <memory>   // for make_unique, unique_ptr
#include <optional>
#include <string>       // for string
#include <string_view>  // for string_view
#include <utility>      // for pair
#include <vector>       // for vector

#include "SocketStream.h"  // for SocketStream
#include "bell/io/URIParser.h"
#include "fmt/core.h"  // for format
#include "picohttpparser.h"

namespace bell::http {
// Type definition for HTTP headers
using ValueHeader = std::pair<std::string, std::string>;

// Type definition for a list of HTTP headers
using Headers = std::vector<ValueHeader>;

// Default timeout for HTTP operations, in milliseconds
const int httpOperationTimeout = 1000;

/**
 * @brief Constructs a HTTP Range header with the given range.
 * 
 * @param from Starting byte of the range
 * @param end Ending byte of the range. If the start is not provided, the end value is the amount of bytes from the end of the file.
 * @return ValueHeader constructed http range header
 */
ValueHeader rangeHeader(std::optional<int32_t> start,
                        std::optional<int32_t> end);

class Response {
 public:
  explicit Response(std::unique_ptr<bell::io::SocketStream> socketStream)
      : socketStream(std::move(socketStream)) {
    // Reserve the response buffer
    this->responseBuffer.reserve(reservedResponseBufferLen);

    // Read the headers
    readHeaders();
  }

  ~Response();

  /**
   * @brief Get all headers from the response
   * @remark This method will return a copy of all the headers in the response. Calling getHeader() is more efficient if you only need a single header.
   * 
   * @return Headers List of all headers in the response
   */
  Headers getAllHeaders() const;

  /**
   * @brief Return the value of the header with the given name
   * 
   * @param headerName Name of the header to get, case-insensitive
   * @return std::string_view Value of the header, or an empty string_view if the header is not found
   */
  std::string_view getHeader(const std::string& headerName) const;

  /**
   * @brief Returns the content length of the response, as specified in the Content-Length header
   * 
   * @return size_t Content length of the response, or 0 if the header is not present
   */
  size_t getContentLength() const;

  /**
   * @brief Returns the status code of the response
   * 
   * @return int Status code of the response, or 0 if the status code is not available
   */
  int getStatusCode() const;

  /**
   * @brief Returns the body of the response as a string_view
   * @remark This method will read the body from the socket stream if it has not been read yet. 
   *
   * @return std::string_view
   */
  std::string_view getBodyStringView();

  /**
   * @brief Returns the body of the response as a vector of bytes
   * @remark This method will read the body from the socket stream if it has not been read yet.
   * 
   * @return std::vector<std::byte> Vector of bytes representing the body
   */
  std::vector<std::byte> getBodyBytes();

  /**
   * @brief Returns a pointer to the body of the response. The length of the body can be obtained using getBodyBytesLength().
   * @remark This method will read the body from the socket stream if it has not been read yet.
   * 
   * @return const char* Pointer to the body of the response
   */
  const char* getBodyBytesPtr();

  /**
   * @brief Returns the amount of bytes stored in the body buffer, which can be obtained using getBodyBytesPtr().
   * 
   * @return size_t Amount of bytes stored in the body buffer, or 0 if the body has not been read yet
   */
  size_t getBodyBytesLength();

  /**
   * @brief Returns the underlying socket stream, which can be used to read the response body directly.
   * @remark This method will transfer ownership of the socket stream to the caller, invalidating the Response object.
   *
   * @return std::unique_ptr<bell::io::SocketStream> Pointer to the socket stream
   */
  std::unique_ptr<bell::io::SocketStream> getStream();

 private:
  std::unique_ptr<bell::io::SocketStream> socketStream;
  bool isValid = true;

  // picohttpparser headers
  std::vector<phr_header> phResponseHeaders;

  // Amount of bytes to reserve for the HTTP response
  const static size_t reservedResponseBufferLen = 2048;

  // Used as a buffer when the response buffer is not provided from
  std::vector<char> responseBuffer;

  // Response status code
  int statusCode = 0;

  // Cached content length
  size_t contentLength = 0;

  // Actual amount of bytes read
  size_t readContentLength = 0;

  // Reads the headers from the socket stream
  void readHeaders();

  // Reads the body from the socket stream
  void readBody();
};

class Connection {
 public:
  explicit Connection(const std::string& url,
                      int timeoutMs = httpOperationTimeout);
  ~Connection();

  /**
   * @brief Sends a HTTP request to the server, with the given method and headers.
   * 
   * @param method HTTP method to use, e.g. "GET" or "POST"
   * @param extraHeaders Additional headers to include in the request
   * @param expectedContentLength Expected content length in bytes, or 0 if the request does not have a body. This is used to set the Content-Length header, if present.
   */
  void sendRequest(const std::string& method, const Headers& extraHeaders,
                   size_t expectedContentLength);

  /**
   * @brief Sends the full body of the request to the server. Call this method after sendRequest() if the request has a body.
   * 
   * @param body Pointer to the body of the request
   * @param length Length of the body in bytes
   */
  void sendFullBody(const std::byte* body, size_t length);

  /**
   * @brief Returns the response object.This method should be called after sendRequest() and sendFullBody().
   * @remark This method will return a unique pointer to the response object, transferring ownership to the caller.
   *
   * @return std::unique_ptr<Response> Pointer to the response object
   */
  std::unique_ptr<Response> getResponse();

  // Other connection-related methods
 private:
  // Internal socket stream
  std::unique_ptr<bell::io::SocketStream> socketStream;

  // Parsed URL
  bell::uri::URI parsedUrl;

  // We only send the request once
  bool requestSent = false;

  // Expected content length in bytes
  size_t expectedContentLength = 0;

  // Default user agent
  std::string userAgent = "bell/0.1";
};

/**
 * @brief Makes a GET request to the given URL.
 * 
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param timeoutMs Timeout in milliseconds
 *
 * @return std::unique_ptr<Response> Pointer to the response object
 */
inline std::unique_ptr<Response> get(const std::string& url,
                                     const Headers& headers = {},
                                     int timeoutMs = httpOperationTimeout) {
  auto connection = std::make_unique<Connection>(url, timeoutMs);
  connection->sendRequest("GET", headers, 0);
  return connection->getResponse();
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
 * @return std::unique_ptr<Response> Pointer to the response object 
 */
inline std::unique_ptr<Response> postRawPtr(
    const std::string& url, Headers headers = {},
    const std::byte* body = nullptr, size_t length = 0,
    int timeoutMs = httpOperationTimeout) {
  auto connection = std::make_unique<Connection>(url, timeoutMs);
  connection->sendRequest("POST", headers, length);
  connection->sendFullBody(body, length);
  return connection->getResponse();
}

/**
 * @brief Makes a GET request to the given URL.
 * 
 * @param url URL to make the request to
 * @param headers Headers to include in the request
 * @param body Body to include in the request, passed as a vector of bytes
 * @param timeoutMs Timeout in milliseconds
 *
 * @return std::unique_ptr<Response> Pointer to the response object
 */
inline std::unique_ptr<Response> post(const std::string& url,
                                      const Headers& headers = {},
                                      const std::vector<std::byte>& body = {},
                                      int timeoutMs = httpOperationTimeout) {
  return postRawPtr(url, headers, body.data(), body.size(), timeoutMs);
}
}  // namespace bell::http