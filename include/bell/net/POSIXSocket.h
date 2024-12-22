#pragma once

#include "bell/net/IpAddress.h"
#include "bell/net/Socket.h"

namespace bell::net {
/**
 * @brief Common socket implementation for UDP and TCP sockets, later extended by specific implementations.
 */
class POSIXSocket : public Socket {
 public:
  POSIXSocket() = default;  ///< Default constructor.

  // Socket interface overrides
  void wrapFd(int fd) override;
  int getFd() override;
  size_t read(uint8_t* buf, size_t len, int timeoutMs = 0) override;
  size_t write(const uint8_t* buf, size_t len, int timeoutMs = 0) override;
  void bind(const std::string& address, uint16_t port) override;
  void listen(int backlog) override;
  void setBlocking(bool blocking) override;
  int poll(int events, int timeoutMs = 0) override;
  bool isOpen() override;
  void close() override;
  std::string getLocalAddress() const override;
  std::string getRemoteAddress() const override;

 private:
  const char* LOG_TAG = "POSIXSocket";

 protected:
  // File descriptor associated with the socket
  int sockFd = -1;

  // Flag indicating if the socket is closed
  bool isClosed = true;

  // Flag indicating if the socket is blocking
  bool isBlocking = false;

  // Flag indicating if the socket is listening
  bool isListening = false;

  // Destination address, as resolved by the connect method
  IpAddress destinationAddress;
};
}  // namespace bell::net
