#pragma once

// Standard includes
#include <netinet/in.h>
#include <cstdint>

#include "bell/net/IpAddress.h"
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
   * @param type The socket type (e.g., SOCK_STREAM for TCP, SOCK_DGRAM for UDP).
   * @param protocol The protocol to be used (e.g., IPPROTO_TCP for TCP, IPPROTO_UDP for UDP).
   * @return Result<> indicating success or failure.
   */
  Result<> createFd(int domain, int type, int protocol = 0);

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
   * @brief Returns the last error code from the socket, using SO_ERROR.
   */
  std::error_code lastError() const;

  // Socket interface overrides
  void setReceiveTimeout(int timeoutMs) override;
  void setSendTimeout(int timeoutMs) override;
  int getFd() override;
  Result<size_t> read(uint8_t* buf, size_t len) override;
  Result<size_t> write(const uint8_t* buf, size_t len) override;
  Result<> bind(const std::string& address, uint16_t port) override;
  void setBlocking(bool blocking) override;
  bool isValid() const override;
  void close() override;

  static const int INVALID_FD = -1;  //  file descriptor

 private:
  const char* LOG_TAG = "POSIXSocket";

  Result<> setOptionImpl(int level, int optionName, const void* optionValue,
                         socklen_t optionLen);

 protected:
  // File descriptor associated with the socket
  int sockFd = INVALID_FD;

  // Flag indicating if the socket is closed
  bool isClosed = true;

  // Flag indicating if the socket is blocking
  bool isBlocking = true;

  // Flag indicating if the socket is listening
  bool isListening = false;

  // Timeout for socket operations, assigned to SO_RCVTIMEO and SO_SNDTIMEO
  int timeoutMs = 0;

  int sockType = -1;  // SOCK_STREAM or SOCK_DGRAM, set by derived classes

  int sockFamily = -1;  // AF_INET or AF_INET6, set by derived classes

  // Destination address, as resolved by the connect method
  IpAddress destinationAddress;
};
}  // namespace bell::net

namespace bell {
using POSIXSocket = net::POSIXSocket;
}
