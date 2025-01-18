#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bell::http {
// Type definition for HTTP headers
using Header = std::pair<std::string, std::string>;

// Type definition for a list of HTTP headers
using Headers = std::vector<Header>;

// Used to differentiate between HTTP Requests and Responses, passed as a parameter for the Reader and Writer constructors
enum class Direction { Request, Response };

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

// Alias for the HTTPCommon class
namespace bell {
using HTTPHeader = http::Header;
using HTTPHeaders = http::Headers;
using HTTPMethod = http::Method;
using HTTPDirection = http::Direction;
};  // namespace bell
