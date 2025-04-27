#pragma once

// Standard includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdint>

#include "bell/net/Socket.h"

namespace bell::net {
/**
 * @brief Common socket implementation for UDP and TCP sockets, later extended by specific implementations.
 */
class POSIXSocket : public Socket {
 public:
  POSIXSocket() = default;

  /**
   * @brief Create a socket file descriptor with the specified family, type and protocol
   *
   * @param domain The address family (e.g., AF_INET for IPv4, AF_INET6 for IPv6).
   * @param protocol The protocol to be used (e.g., IPPROTO_TCP for TCP, IPPROTO_UDP for UDP).
   * @return Result<> indicating success or failure.
   */
  Result<> createFd(int domain, int protocol = 0);

  /**
   * @brief Set a socket option with a templated value.
   *
   * This method wraps the setsockopt function to set various socket options,
   * inferring the value's size based on the type of optionValue.
   *
   * @param level The level at which the option is defined (e.g., SOL_SOCKET).
   * @param optionName The name of the option to be set (e.g., SO_REUSEADDR).
   * @param optionValue The value of the option to be set.
   */
  template <typename T>
  Result<> setOption(int level, int optionName, const T& optionValue) {
    return setOptionImpl(level, optionName, &optionValue, sizeof(T));
  }

  /**
   * @brief Get a socket option with a templated value.
   *
   * This method wraps the getsockopt function to retrieve various socket options, inferring the value's size based on the type of optionValue.
   *
   * @param level The level at which the option is defined (e.g., SOL_SOCKET).
   * @param optionName The name of the option to be retrieved (e.g., SO_REUSEADDR).
   * @return Result<T> containing the value of the option or an error code.
   */
  template <typename T>
  Result<T> getOption(int level, int optionName) {
    T optionValue{};
    socklen_t optionLen = sizeof(T);

    auto res = getOptionImpl(level, optionName, &optionValue, optionLen);

    if (!res) {
      return Result<T>::fromError(res.getError());
    }

    return Result<T>(optionValue);
  }

  /**
   * @brief Bind the socket to a specific address and port.
   *
   * @param address A string representation of the address to bind to (e.g., "127.0.0.1").
   * @param port The port number to bind to, or 0 for a random port.
   * @param reuseAddr If true, allows the socket to bind to an address that is already in use.
   *
   * @return Result<int> resulting port number or an error code.
   */
  Result<int> bind(const std::string& address, uint16_t port,
                   bool reuseAddr = true);

  /**
   * @brief Returns the last error code from the socket, using SO_ERROR.
   */
  std::error_code lastError() const;

  // Socket interface overrides
  Result<> setReceiveTimeout(int timeoutMs) override;
  Result<> setSendTimeout(int timeoutMs) override;
  Result<int> getReceiveTimeout() override;
  Result<int> getSendTimeout() override;
  Result<size_t> read(uint8_t* buf, size_t len) override;
  Result<size_t> write(const uint8_t* buf, size_t len) override;
  Result<> setBlocking(bool blocking) override;
  Result<bool> getBlocking() const override;
  bool isValid() const override;
  void close() override;
  int takeFd() override;
  int getFd() const override;

  // To be implemented by derived classes
  virtual int getSockType() = 0;

  // Default value for invalid file descriptor
  static const int INVALID_FD = -1;

 private:
  const char* LOG_TAG = "POSIXSocket";

  Result<> setOptionImpl(int level, int optionName, const void* optionValue,
                         socklen_t optionLen);

  Result<> getOptionImpl(int level, int optionName, void* optionValue,
                         socklen_t optionLen);

 protected:
  // File descriptor associated with the socket
  int sockFd = INVALID_FD;
};
}  // namespace bell::net

namespace bell {
using POSIXSocket = net::POSIXSocket;
}
