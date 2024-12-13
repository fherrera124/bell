
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
      : socketStream(std::move(socketStream)) {}
  ~Response();

  void readHeaders();

  void readBody();

  Headers getAllHeaders();

  std::string_view getHeader(const std::string& headerName);

  size_t getContentLength();

  int getStatusCode();

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
};

class Connection {
 public:
  explicit Connection(const std::string& url, int timeoutMs = 0);
  ~Connection();

  void sendRequest(const std::string& method, const Headers& extraHeaders,
                   size_t expectedContentLength);

  void sendFullBody(const std::byte* body, size_t length);

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
// class HTTPClient {
//  public:
//   // most basic header type, represents by a key-val
//   using ValueHeader = std::pair<std::string, std::string>;

//   using Headers = std::vector<ValueHeader>;

//   // Helper over ValueHeader, formatting a HTTP bytes range
//   struct RangeHeader {
//     static ValueHeader range(int32_t from, int32_t to) {
//       return ValueHeader{"Range", fmt::format("bytes={}-{}", from, to)};
//     }

//     static ValueHeader last(int32_t nbytes) {
//       return ValueHeader{"Range", fmt::format("bytes=-{}", nbytes)};
//     }
//   };

//   class Response {
//    public:
//     Response() = default;
//     ~Response();

//     /**
//     * Initializes a connection with a given url.
//     */
//     void connect(const std::string& url);

//     void rawRequest(const std::string& method, const std::string& url,
//                     const std::vector<uint8_t>& content, Headers& headers);
//     void get(const std::string& url, Headers headers = {});
//     void post(const std::string& url, Headers headers = {},
//               const std::vector<uint8_t>& body = {});

//     std::string_view body();
//     std::vector<uint8_t> bytes();

//     std::string_view header(const std::string& headerName);
//     bell::io::SocketStream& stream();
//     size_t contentLength();
//     size_t totalLength();

//    private:
//     bell::io::SocketStream socketStream;

//     const size_t HTTP_BUF_SIZE = 1024;

//     std::vector<uint8_t> httpBuffer = std::vector<uint8_t>(HTTP_BUF_SIZE);
//     std::vector<uint8_t> rawBody = std::vector<uint8_t>();
//     size_t httpBufferAvailable = 0;
//     size_t contentSize = 0;
//     bool hasContentSize = false;

//     Headers responseHeaders = {};

//     void readResponseHeaders();
//     void readRawBody();
//   };

//   enum class Method : uint8_t { GET = 0, POST = 1 };

//   struct Request {
//     std::string url;
//     Method method;
//     Headers headers;
//   };

//   static std::unique_ptr<Response> get(const std::string& url,
//                                        Headers headers = {}) {
//     auto response = std::make_unique<Response>();
//     response->connect(url);
//     response->get(url, std::move(headers));
//     return response;
//   }

//   static std::unique_ptr<Response> post(const std::string& url,
//                                         Headers headers = {},
//                                         const std::vector<uint8_t>& body = {}) {
//     auto response = std::make_unique<Response>();
//     response->connect(url);
//     response->post(url, std::move(headers), body);
//     return response;
//   }
// };
}  // namespace bell::http