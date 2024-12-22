#pragma once

#include "bell/net/POSIXSocket.h"

namespace bell::net {
/**
 * @brief UDP implementation of the net::Socket
 */
class UDPSocket : public POSIXSocket {
 public:
  UDPSocket() = default;
  ~UDPSocket() override;

  // Socket interface overrides
  // Please note that UDP socket does not exactly "connect" to a host, but rather binds to the destination address
  void connect(const std::string& host, uint16_t port,
               int timeoutMs = 0) override;
  std::unique_ptr<Socket> accept() override {
    return nullptr;
  }  // Not implemented for UDP
     // UDP specific methods
     // TODO: implement sendto and recvfrom

 private:
  const char* LOG_TAG = "UDPSocket";
};
}  // namespace bell::net
