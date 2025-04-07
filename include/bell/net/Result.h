#pragma once

#include <sys/errno.h>
#include <system_error>

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
template <typename T>
class Result {
 public:
  // Success
  Result(T value)
      : value(std::move(value)),
        error({static_cast<int>(Error::Success), std::generic_category()}) {}

  // Error from errno
  Result(int err) : error(errorFromPosix(err)) {}

  // Explicit NetworkError
  Result(Error err) : error(static_cast<int>(err), std::system_category()) {}

  bool success() const { return error.value() == 0; }
  const T& getValue() const { return value; }
  std::error_code getError() const { return error; }

  // Throw on error (optional)
  T unwrap() const {
    if (!success())
      throw std::system_error(error);
    return value;
  }

 private:
  T value;
  std::error_code error;
};

}  // namespace bell::net