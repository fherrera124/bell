#pragma once

#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "bell/Result.h"
#include "bell/http/Common.h"
#include "bell/http/Reader.h"
#include "bell/net/URIParser.h"
#include "tcb/span.hpp"

// Go-lang inspired http client
namespace bell::http {

/**
 * @brief A variant representing the body of an HTTP request.
 *
 * It can be empty (std::monostate), a pre-filled byte vector, or a stream for large bodies.
 */
using RequestBody =
    std::variant<std::monostate, std::vector<std::byte>, tcb::span<std::byte>,
                 std::string_view, std::istream*>;

/**
 * @brief Represents an HTTP request.
 *
 * Contains all necessary information to send a request, such as method, URI,
 * headers, and body.
 */
class Request {
 public:
  Request() = default;
  ~Request() = default;

  /**
   * @brief Factory function to create a Request object.
   * @param method The HTTP method (e.g., GET, POST).
   * @param url The target URL for the request.
   * @return A Result containing the Request on success, or an error if the URL is invalid.
   */
  static bell::Result<Request> create(http::Method method,
                                      const std::string& url);

  /// The HTTP method for the request.
  http::Method method;
  /// The parsed URI for the request.
  bell::net::URI uri;
  /// A map of HTTP headers.
  Headers headers;

  /// The request body, which can be empty, a byte vector, or a stream.
  RequestBody body;

  /// The explicit length of the request body, if known.
  std::optional<size_t> contentLength;

  /// Timeout for the entire HTTP operation in milliseconds.
  std::optional<int> operationTimeoutMs;

 private:
  /**
   * @brief Private constructor to enforce creation via the static `create` method.
   * @param method The HTTP method.
   * @param parsedUrl A successfully parsed URI object.
   */
  Request(http::Method method, bell::net::URI parsedUrl)
      : method(method), uri(std::move(parsedUrl)) {}
};

/**
 * @brief Represents an HTTP response received from a server.
 *
 * Provides access to the status code, headers, and the response body.
 */
class Response {
 public:
  /// The HTTP status code (e.g., 200, 404).
  int statusCode = 0;
  /// The HTTP status message (e.g., "OK", "Not Found").
  std::string statusMessage;
  /// The response headers.
  Headers headers;
  /// The length of the response body, if provided by the server.
  std::optional<size_t> contentLength;
  /// A reader to access the response body.
  Reader bodyReader;

  /**
   * @brief Constructs a Response from a response reader.
   * @param responseReader The reader providing the raw response data.
   */
  Response(http::Reader responseReader);
  ~Response() = default;

  /**
   * @brief Reads the entire response body as a string.
   * @return A Result containing a string_view of the body on success.
   */
  bell::Result<std::string_view> text();

  /**
   * @brief Reads the entire response body as a vector of bytes.
   * @return A Result containing a vector of bytes on success.
   */
  bell::Result<std::vector<std::byte>> bytes();

  /**
   * @brief Gets a pointer to the raw byte buffer of the response body.
   * @return A Result containing a const char* pointer to the data on success.
   */
  bell::Result<const char*> bytesPtr();

  /**
   * @brief Gets the length of the response body.
   * @return A Result containing the length of the body on success.
   */
  bell::Result<size_t> bytesLength();

  /**
   * @brief Gets the underlying stream for reading the response body.
   * @return A pointer to the std::istream.
   */
  std::istream* stream() const;
};

/**
 * @brief An abstract interface for executing HTTP requests.
 *
 * This allows for different underlying HTTP implementations (e.g., mock, curl).
 */
class Transport {
 public:
  virtual ~Transport() = default;

  /**
   * @brief Executes an HTTP request.
   * @param req The request to execute.
   * @return A Result containing the Response on success, or an error.
   */
  virtual bell::Result<Response> execute(const Request& req) = 0;
};

/**
 * @brief The default transport implementation for sending HTTP requests.
 */
class DefaultTransport : public Transport {
 public:
  DefaultTransport() = default;

  /**
   * @brief Executes an HTTP request using the default underlying implementation.
   * @param req The request to execute.
   * @return A Result containing the Response on success, or an error.
   */
  bell::Result<Response> execute(const Request& req) override;
};

/**
 * @brief A high-level HTTP client for making requests.
 *
 * Provides convenient methods like get(), post(), etc., to interact with web services.
 */
class Client {
 public:
  /// Default timeout for all operations in milliseconds.
  int operationTimeoutMs = 5000;

  /**
   * @brief Constructs a Client with a specific transport layer.
   * @param transport The transport implementation to use for requests.
   * Defaults to DefaultTransport.
   */
  explicit Client(std::unique_ptr<Transport> transport =
                      std::make_unique<DefaultTransport>())
      : transport(std::move(transport)) {}

  /**
   * @brief Sends a raw HTTP request using the provided Request object.
   * @param req The Request object containing the HTTP method, URL, headers, and body.
   * @return A Result containing the Response on success, or an error code on failure.
   */
  bell::Result<Response> rawRequest(Request& req);

  /**
   * @brief Sends a GET request to the specified URL with optional headers.
   * @param url The URL to send the GET request to.
   * @param headers Optional headers to include in the request.
   * @return A Result containing the Response on success, or an error code on failure.
   * @note The URL should be a valid HTTP or HTTPS URL.
   */
  bell::Result<Response> get(const std::string& url,
                             const Headers& headers = {});

  /**
   * @brief Sends a POST request to the specified URL.
   * @param url The URL to send the POST request to.
   * @param headers Optional headers to include in the request.
   * @param body The request body. Can be empty, a byte vector, or a stream.
   * @param bodyLength The length of the body, required for streams.
   * @return A Result containing the Response on success, or an error.
   */
  bell::Result<Response> post(const std::string& url,
                              const Headers& headers = {},
                              RequestBody body = std::monostate(),
                              std::optional<size_t> bodyLength = std::nullopt);

  /**
   * @brief Sends a PUT request to the specified URL.
   * @param url The URL to send the PUT request to.
   * @param headers Optional headers to include in the request.
   * @param body The request body. Can be empty, a byte vector, or a stream.
   * @param bodyLength The length of the body, required for streams.
   * @return A Result containing the Response on success, or an error.
   */
  bell::Result<Response> put(const std::string& url,
                             const Headers& headers = {},
                             RequestBody body = std::monostate(),
                             std::optional<size_t> bodyLength = std::nullopt);

 private:
  /// The transport layer used to execute requests.
  std::unique_ptr<Transport> transport;
};
}  // namespace bell::http

namespace bell {
using HTTPClient = http::Client;
using HTTPResponse = http::Response;
using HTTPRequest = http::Request;
using HTTPTransport = http::Transport;
}  // namespace bell
