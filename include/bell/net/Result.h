#pragma once

#include <sys/errno.h>
#include <optional>
#include <system_error>
#include <variant>

namespace bell::net {
enum class Error {
  // Protocol-level errors (HTTP/WebSockets/DNS)
  InvalidHeader,
  InvalidResponse,
  RedirectLoop,
  DnsResolutionFailed,
};

/**
 * @brief Network-specific result type (usable by sockets, HTTP, etc.).
 */
template <typename T = std::monostate>
class Result {
 public:
  // Success
  Result() = default;

  Result(const T& val) : value(val){};

  Result(T&& val) : value(std::move(val)){};

  // Result(const std::error_code& err) : error(err) {};

  Result(std::error_code err) : error(err){};

  static Result fromError(std::errc err) {
    return Result(std::make_error_code(err));
  }

  static Result fromError(const std::error_code& err) { return Result(err); }

  static Result fromLastErrno() {
    int err = errno;
    return Result({err, std::system_category()});
  }

  static Result fromError(int ec, const std::error_category& category) {
    return Result({ec, category});
  }

  bool isSuccess() const { return !error; }
  const T& getValue() const { return value; }

  T&& takeValue() {
    if (!isSuccess())
      throw std::system_error(error);
    return std::move(value);
  }

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