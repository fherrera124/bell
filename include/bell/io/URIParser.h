#pragma once
#include <string>  // for std::string

namespace bell::uri {

/**
 * @brief Encode a string to URL format (percent encoding)
 * 
 * @param value The string to encode
 * @return std::string The encoded string
 */
std::string encode(std::string_view value);

/**
 * @brief Decode a URL-encoded string
 * 
 * @param value The URL-encoded string
 * @return std::string The decoded string
 */
std::string decode(std::string_view value);

// Struct to hold the parsed URL components
struct ParsedURI {
  std::optional<std::string> scheme;  // (http, https, etc.)
  std::optional<std::string> host;    // Hostname or IP address
  std::optional<int> port;            // Port number
  std::optional<std::string> path;    // Path component of the URL
  std::optional<std::string> query;   // Query component of the URL
};

/**
 * @brief Parse a URI into its components
 * 
 * @param uri The URI to parse
 * @return std::optional<ParsedURI> The parsed URI components, or std::nullopt if parsing failed
 */
std::optional<ParsedURI> parse(std::string_view uri);

}  // namespace bell::url