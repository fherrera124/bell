#pragma once

// System includes
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Library includes
#include <picohttpparser.h>

// Own includes
#include "bell/http/Common.h"

namespace bell::http {
class Reader {
 public:
  /**
   * @brief HTTPReader constructor, initializes the reader with the given stream. No data is read from the stream until readHeaders() is called.
   *
   * @param readerDirection Type of the reader, either Request or Response
   * @param istream Pointer to the input stream, which must be valid until the reader is destroyed
   * @param externalBuffer Optional pointer to an external buffer to use for reading. If not provided, an internal buffer will be used. The buffer will be expanded as needed.
   */
  Reader(Direction readerDirection, std::istream* istream,
         std::vector<char>* externalBuffer = nullptr);

  /**
   * @brief Read the headers from the stream. This method needs to be called before any other methods.
   */
  void readHeaders();

  /**
   * @brief Return the value of the header with the given name
   *
   * @param headerName Name of the header to get, case-insensitive
   * @return std::string_view Value of the header, or an empty string_view if the header is not found
   */
  std::string_view getHeader(const std::string& headerName) const;

  /**
   * @brief Get all headers from the response
   * @remark This method will return a copy of all the headers in the response. Calling getHeader() is more efficient if you only need a single header.
   *
   * @return Headers List of all headers in the response
   */
  Headers getAllHeaders() const;

  /**
   * @brief Returns the content length as specified in the Content-Length header
   *
   * @return size_t Content length of the response, or 0 if the header is not present
   */
  size_t getContentLength() const;

  /**
   * @brief Returns the status code of the response
   * @remark Only valid for response readers
   */
  int getStatusCode() const;

  /**
   * @brief Returns the path of the request, e.g., /index.html
   * @remark Only valid for request readers
   */
  std::string_view getPath() const;

  /**
   * @brief Returns the HTTP method of the request
   * @remark Only valid for request readers
   */
  Method getMethod() const;

  /**
   * @brief Returns the body of the response as a string_view
   * @remark This method will read the body from the socket stream if it has not been read yet.
   *
   * @return std::string_view
   */
  std::string_view getBodyStringView();

  /**
   * @brief Returns the body of the response as a vector of bytes
   * @remark This method will read the body from the stream if it has not been read yet.
   *
   * @return std::vector<std::byte> Vector of bytes representing the body
   */
  std::vector<std::byte> getBodyBytes();

  /**
   * @brief Returns a pointer to the body of the response. The length of the body can be obtained using getBodyBytesLength()
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
   * @brief Returns the query parameters of the request, parsed as key-value pairs
   * 
   * @return std::unordered_map<std::string, std::string> 
   */
  std::unordered_map<std::string, std::string> getQueryParams() const;

  /**
   * @brief Returns the stream used by the reader
   *
   * @return std::istream* Pointer to the stream used by the reader. Will be valid until the reader is destroyed.
   */
  std::istream* getStream() const;

 private:
  static const int maxRequestLen = 4 * 1024;
  Direction readerDirection;
  std::istream* istream;
  std::vector<char> internalBuffer;
  std::vector<char>* bufferPtr;

  bool headersValid = false;
  size_t readContentLength = 0;

  // picohttpparser headers
  std::vector<phr_header> phrHeaders;

  std::optional<size_t> contentLength;

  // Request specific fields
  std::optional<Method> method;
  std::optional<std::string_view> path;
  std::unordered_map<std::string, std::string> queryParams;

  // Response specific fields
  std::optional<int> statusCode;
  std::optional<std::string_view> statusMessage;

  // Tries to parse the query parameters from path, stripping it if successful
  void parseQueryParams();

  // Ensure the response is of the expected type, throws an exception if it is not
  void ensureValid(Direction expectedDirection) const;

  void readBody();
};
}  // namespace bell::http
