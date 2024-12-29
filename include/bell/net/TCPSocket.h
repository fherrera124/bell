#pragma once

#include <memory>

#include "bell/net/POSIXSocket.h"

namespace bell::net {
/**
 * @brief TCP implementation of the bell::Socket
 */
class TCPSocket : public POSIXSocket {
 public:
  TCPSocket() { this->sockType = SOCK_STREAM; };
  ~TCPSocket() override;

  /**
   * @brief Resolve the provided host and port, and attempt to create a socket connected there.
   *
   * This method resolves the hostname and attempts to connect to the specified port.
   *
   * @param host String containing a hostname or IP address to connect to.
   * @param port The port number to connect to on the specified host.
   * @param timeout The maximum time to wait for the connection to be established, in milliseconds. This parameter is ignored, if the socket is set to a blocking mode.
   */
  void connect(const std::string& host, uint16_t port, int timeoutMs = 0);

  /**
   * @brief Listen for incoming connections on the socket.
   *
   * @param backlog The maximum number of pending connections to allow.
   */
  void listen(int backlog = 5);

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
