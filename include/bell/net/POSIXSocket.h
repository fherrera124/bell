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
  void setOption(int level, int optionName, const T& optionValue) {
    setOptionImpl(level, optionName, &optionValue, sizeof(T));
  }

  /**
   * @brief Create an underlying file descriptor for the socket.
   *
   * This method initializes the file descriptor, using the provided domain and protocol.
   *
   * @remark This is done internally by bind, connect, and listen methods. Only call this method if you need to create a socket without binding, connecting, or listening.
   * @param domain The domain of the socket (e.g., AF_INET).
   * @param protocol The protocol of the socket (e.g., SOCK_STREAM).
   */
  void createFd(int domain, int protocol = IPPROTO_IP);

  // Socket interface overrides
  void wrapFd(int fd) override;
  int getFd() override;
  size_t read(uint8_t* buf, size_t len, int timeoutMs = 0) override;
  size_t write(const uint8_t* buf, size_t len, int timeoutMs = 0) override;
  void bind(const std::string& address, uint16_t port) override;
  void setBlocking(bool blocking) override;
  int poll(int events, int timeoutMs = 0) override;
  bool isOpen() override;
  void close() override;
  std::string getLocalAddress() const override;
  std::string getRemoteAddress() const override;

 private:
  const char* LOG_TAG = "POSIXSocket";

  void setOptionImpl(int level, int optionName, const void* optionValue,
                     socklen_t optionLen);

 protected:
  // File descriptor associated with the socket
  int sockFd = -1;

  // Flag indicating if the socket is closed
  bool isClosed = true;

  // Flag indicating if the socket is blocking
  bool isBlocking = false;

  // Flag indicating if the socket is listening
  bool isListening = false;

  int sockType = -1;  // SOCK_STREAM or SOCK_DGRAM, set by derived classes

  // Destination address, as resolved by the connect method
  IpAddress destinationAddress;
};
}  // namespace bell::net

namespace bell {
using POSIXSocket = net::POSIXSocket;
}
