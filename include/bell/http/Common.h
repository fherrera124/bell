#pragma once

#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace bell::http {

enum class Errc { Success = 0, InvalidURL, InvalidState, SocketNotOpen };

namespace internal {
struct http_error_category : public std::error_category {
  const char* name() const noexcept override { return "BellHTTP"; }
  std::string message(int ev) const noexcept override {
    switch (static_cast<Errc>(ev)) {
      case Errc::Success:
        return "Success";
      case Errc::InvalidURL:
        return "Invalid URL";
      case Errc::InvalidState:
        return "Invalid state, either the request or response is not valid";
      case Errc::SocketNotOpen:
        return "Socket not open";
      default:
        return "Unknown error";
    }
  }
};
}  // namespace internal

// Plug in the error code category for std::error_code
inline std::error_code make_error_code(const bell::http::Errc& e) {
  return {static_cast<int>(e), internal::http_error_category()};
};

// Type definition for HTTP headers
using Header = std::pair<std::string, std::string>;

// Type definition for a list of HTTP headers
using Headers = std::vector<Header>;

// Used to differentiate between HTTP Requests and Responses, passed as a parameter for the Reader and Writer constructors
enum class Direction { Request, Response, Invalid };

// HTTP Method enumeration
enum class Method { GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH, INVALID };

/**
 * @brief Parse the HTTP method from a string
 *
 * @param method The method string to parse
 * @return HTTPMethod The parsed HTTP method, or HTTPMethod::INVALID if the method is not recognized
 */
Method parseMethod(std::string_view method);

/// @brief Returns a string representation of the HTTP method
std::string_view methodToString(Method method);
}  // namespace bell::http

namespace std {
template <>
struct is_error_code_enum<bell::http::Errc> : true_type {};
}  // namespace std

// Alias for the HTTPCommon class
namespace bell {
using HTTPHeader = http::Header;
using HTTPHeaders = http::Headers;
using HTTPMethod = http::Method;
using HTTPDirection = http::Direction;
};  // namespace bell
