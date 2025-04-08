#pragma once

#include <memory>

#include "bell/net/POSIXSocket.h"

namespace bell::net {
/**
 * @brief TCP implementation of the bell::Socket
 */
class TCPSocket : public POSIXSocket {
 public:
  /**
  * @brief Default constructor for the TCPSocket class. Initializes the socket to INVALID_FD.
  */
  TCPSocket() = default;
  /**
   * @brief Constructor that wraps an existing file descriptor.
   *
   * @param wrapFd The file descriptor to wrap.
   * @param family The address family (e.g., AF_INET, AF_INET6). Default is AF_UNSPEC.
   * @param sockType The socket type (e.g., SOCK_STREAM). Default is SOCK_STREAM.
   */
  TCPSocket(int wrapFd, int family = AF_UNSPEC, int sockType = SOCK_STREAM) {
    this->sockFd = wrapFd;
    this->sockFamily = family;
    this->sockType = sockType;
  }

  ~TCPSocket() override;

  /**
   * @brief Resolve the provided host and port, and attempt to create a socket connected there.
   *
   * This method resolves the hostname and attempts to connect to the specified port. It will also set the default timeout for the socket.
   *
   * @param host String containing a hostname or IP address to connect to.
   * @param port The port number to connect to on the specified host.
   * @param timeout The maximum time to wait for the connection to be established, in milliseconds
   */
  Result<> connect(const std::string& host, uint16_t port, int timeoutMs = 0);

  /**
   * @brief Listen for incoming connections on the socket.
   *
   * @param backlog The maximum number of pending connections to allow.
   */
  Result<> listen(int backlog = 5);

  /**
   * @brief Accept an incoming connection on the socket.
   *
   * @return std::unique_ptr<TCPSocket> A unique pointer to the accepted socket.
   */
  std::unique_ptr<TCPSocket> accept();

 private:
  const char* LOG_TAG = "TCPSocket";
};
}  // namespace bell::net

namespace bell {
using TCPSocket = net::TCPSocket;
}
