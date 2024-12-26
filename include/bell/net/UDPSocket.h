#pragma once

#include "bell/net/POSIXSocket.h"

namespace bell::net {
/**
 * @brief UDP implementation of the net::Socket
 */
class UDPSocket : public POSIXSocket {
 public:
  UDPSocket() { this->sockType = SOCK_DGRAM; };
  ~UDPSocket() override;

  // UDP specific methods
  // TODO: implement sendto and recvfrom

 private:
  const char* LOG_TAG = "UDPSocket";
};
}  // namespace bell::net
