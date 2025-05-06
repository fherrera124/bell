#pragma once

#include <cerrno>
#include <system_error>
#include <variant>

namespace bell {

/**
 * @brief A Result class that encapsulates a value or an error code.
 */
template <typename T = std::monostate>
class Result {
 public:
  // Success
  Result() = default;

  template <typename U = T,
            typename = std::enable_if_t<
                !std::is_same_v<std::decay_t<U>, std::error_code> &&
                !std::is_same_v<std::decay_t<U>, std::errc>>>
  Result(U&& val) : value(std::forward<U>(val)) {}

  // Error cases - explicit handling
  Result(std::error_code err) : error(err) {}
  Result(std::errc err) : error(std::make_error_code(err)) {}

  // Special case for errno-style errors
  Result(int ec, const std::error_category& category) : error(ec, category) {}

  static Result fromError(std::errc err) {
    return Result(std::make_error_code(err));
  }

  static Result fromError(const std::error_code& err) { return Result(err); }

  static Result fromLastErrno() {
    int err = errno;
    return Result(err, std::system_category());
  }

  static Result fromError(int ec, const std::error_category& category) {
    return Result(ec, category);
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

  T&& unwrapAndTake() {
    if (!isSuccess())
      throw std::system_error(error);
    return std::move(value);
  }

  // override the boolean operator
  explicit operator bool() const { return isSuccess(); }

 private:
  T value = {};
  std::error_code error;
};
}  // namespace bell
