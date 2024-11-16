#pragma once

#include "POSIXSocket.h"

namespace bell::io {
/**
 * @brief TCP implementation of the bell::Socket
 */
class TCPSocket : public POSIXSocket {
 public:
  TCPSocket() = default;
  ~TCPSocket() override;

  // Socket interface overrides
  void connect(const std::string& host, uint16_t port,
               int timeoutMs = 0) override;
  std::unique_ptr<Socket> accept() override;

 private:
  const char* LOG_TAG = "TCPSocket";
};
}  // namespace bell::io