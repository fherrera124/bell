#include "bell/io/URIParser.h"

// System includes
#include <sstream>
#include <string>

using namespace bell;

namespace {
const std::string hexDigitsStr = "0123456789ABCDEF";
};

std::string bell::uri::encode(const std::string_view value) {
  std::stringstream result;

  for (char ch : value) {
    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' || ch == '!' ||
        ch == '\'' || ch == '(' || ch == ')' || ch == '*' || ch == '~' ||
        ch == '.') {
      result.put(ch);
    } else {
      result.put('%');
      result.put(hexDigitsStr[ch >> 4]);
      result.put(hexDigitsStr[ch & 0xF]);
    }
  }
  return result.str();
}

std::string bell::uri::decode(const std::string_view value) {
  std::stringstream result;

  for (std::size_t i = 0; i < value.size(); ++i) {
    char ch = value[i];
    if (ch == '%' && i + 2 < value.size()) {
      std::string_view hex = value.substr(i + 1, 2);
      char dec = static_cast<char>(std::strtol(hex.data(), nullptr, 16));
      result.put(dec);
      i += 2;
    } else if (ch == '+') {
      result.put(' ');
    } else {
      result.put(ch);
    }
  }
  return result.str();
}

std::optional<bell::uri::ParsedURI> bell::uri::parse(std::string_view uri) {
  bell::uri::ParsedURI result;

  // Parse the scheme
  auto scheme_end = uri.find("://");
  if (scheme_end == std::string_view::npos || scheme_end == 0) {
    return std::nullopt;  // No scheme found or scheme is empty
  }

  result.scheme = std::string(uri.substr(0, scheme_end));
  uri.remove_prefix(scheme_end + 3);

  // Parse the authority part (host and port)
  auto authority_end = uri.find_first_of("/?");
  if (authority_end == std::string_view::npos)
    authority_end = uri.length();  // End of string, if no path/query present

  auto authority = uri.substr(0, authority_end);

  auto colon_pos = authority.find(':');
  if (colon_pos != std::string_view::npos) {
    // Host is mandatory, port is optional
    if (colon_pos == 0 || colon_pos == authority.length() - 1) {
      return std::nullopt;  // Malformed authority
    }
    result.host = std::string(authority.substr(0, colon_pos));

    try {
      result.port = std::stoi(std::string(authority.substr(colon_pos + 1)));
    } catch (const std::exception&) {
      return std::nullopt;  // Invalid port
    }
  } else {
    if (authority.empty()) {
      return std::nullopt;  // Missing host
    }
    result.host = std::string(authority);
  }

  uri.remove_prefix(authority_end);

  // Parse the path
  if (!uri.empty() && uri.front() == '/') {
    auto path_end = uri.find('?');
    result.path = std::string(uri.substr(0, path_end));
    uri.remove_prefix(path_end);
  } else {
    result.path = "/";  // Default path if none specified
  }

  // Parse the query
  if (!uri.empty() && uri.front() == '?') {
    uri.remove_prefix(1);
    if (uri.empty()) {
      return std::nullopt;  // Query starts with '?' but is empty
    }
    result.query = std::string(uri);
  }

  return result;
}
