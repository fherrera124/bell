#pragma once

#include <sys/errno.h>
#include <optional>
#include <system_error>
#include <variant>

namespace bell::net {
enum class Error {
  Success = 0,
  // Low-level socket errors (map to errno)
  WouldBlock,         // EWOULDBLOCK / EAGAIN
  ConnectionReset,    // ECONNRESET
  TimedOut,           // ETIMEDOUT
  AddressInUse,       // EADDRINUSE
  ConnectionRefused,  // ECONNREFUSED
  SocketNotOpen,

  // Protocol-level errors (HTTP/WebSockets/DNS)
  InvalidHeader,
  InvalidResponse,
  RedirectLoop,
  DnsResolutionFailed,
};

/**
 * @brief Convert NetworkError + errno into std::error_code.
 */
inline std::error_code errorFromPosix(int err = 0) {
  if (err == 0)
    err = errno;

  switch (err) {
    case 0:
      return {static_cast<int>(Error::Success), std::system_category()};
    case EWOULDBLOCK:
      return {static_cast<int>(Error::WouldBlock), std::system_category()};
    case ECONNRESET:
      return {static_cast<int>(Error::ConnectionReset), std::system_category()};
    case ETIMEDOUT:
      return {static_cast<int>(Error::TimedOut), std::system_category()};
    // ... Add other errnos
    default:
      return {err, std::generic_category()};
  }
}

/**
 * @brief Network-specific result type (usable by sockets, HTTP, etc.).
 */
template <typename T = std::monostate>
class Result {
 public:
  // Success
  Result() = default;

  Result(const T& val) : value(val) {};

  Result(T&& val) : value(std::move(val)) {};

  Result(const std::error_code& err) : error(err) {};

  static Result fromError(const std::error_code& err) { return Result(err); }

  static Result fromLastErrno() {
    int err = errno;
    return Result({err, std::system_category()});
  }

  bool isSuccess() const { return !error; }
  const T& getValue() const { return value; }
  std::error_code getError() const { return error; }

  std::string errorMessage() const { return error.message(); }

  // Throw on error (optional)
  T unwrap() const {
    if (!isSuccess())
      throw std::system_error(error);
    return value;
  }

  // override the boolean operator
  explicit operator bool() const { return isSuccess(); }

 private:
  T value = {};
  std::error_code error;
};

}  // namespace bell::net