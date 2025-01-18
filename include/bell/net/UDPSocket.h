#pragma once

#include "bell/net/IpAddress.h"
#include "bell/net/POSIXSocket.h"

namespace bell::net {
/**
 * @brief UDP implementation of the net::Socket
 */
class UDPSocket : public POSIXSocket {
 public:
 /**
  * @brief UDP socket constructor
  * 
  * @param initAsIpV6 
  */
  UDPSocket(bool initAsIpV6 = false) { this->sockType = SOCK_DGRAM; };
  ~UDPSocket() override;

  /**
   * @brief Receive data from the provided address
   *
   * This method receives data from the socket and stores it in the provided buffer.
   * The method blocks until data is available or an error occurs. The return value
   * indicates the number of bytes successfully read.
   *
   * @param buf Pointer to the buffer where the received data will be stored.
   * @param len The maximum number of bytes to read into the buffer.
   * @param address The address from which the data should be received.
   * @param timeoutMs The maximum time to wait for data to become available, in milliseconds. This parameter is ignored, if the socket is set to a blocking mode.
   * @return The number of bytes successfully read. A return value of 0 may indicate
   * that the connection was closed, while a value less than len could indicate that
   * no more data is currently available.
   */
  size_t recvfrom(uint8_t* buf, size_t len, const IpAddress& address,
                  int timeoutMs = 0);

  /**
   * @brief Send data to the provided address
   *
   * This method sends data from the provided buffer to the socket. The method
   * blocks until the data is sent or an error occurs. The return value indicates
   * the number of bytes successfully written.
   *
   * @param buf Pointer to the buffer containing the data to send.
   * @param len The number of bytes to write from the buffer.
   * @param address The address to which the data should be sent.
   * @param timeoutMs The maximum time to wait for the write operation to complete, in milliseconds. This parameter is ignored, if the socket is set to a blocking mode.
   * @return The number of bytes successfully written.
   */
  size_t sendto(const uint8_t* buf, size_t len, const IpAddress& address,
                int timeoutMs = 0);

 private:
  const char* LOG_TAG = "UDPSocket";
};
}  // namespace bell::net

namespace bell {
using UDPSocket = net::UDPSocket;
}
