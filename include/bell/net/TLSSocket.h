#pragma once

// Standard includes
#include <netinet/in.h>
#include <cstdint>

// MbedTLS includes
#include "bell/net/TCPSocket.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"

#include "bell/net/Socket.h"

namespace bell::net {
/**
 * @brief TLSSocket implementation of the bell::Socket, using MbedTLS.
 */
class TLSSocket : public Socket {
 public:
  TLSSocket();
  ~TLSSocket() override;

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
   * @brief Resolve the provided host and port, and attempt to create a socket connected there.
   *
   * This method resolves the hostname and attempts to connect to the specified port. It will also set the default timeout for the socket.
   *
   * @param host String containing a hostname or IP address to connect to.
   * @param port The port number to connect to on the specified host.
   * @param timeout The maximum time to wait for the connection to be established, in milliseconds. This parameter is ignored, if the socket is set to a blocking mode.
   */
  void connect(const std::string& host, uint16_t port, int timeoutMs = 0);

  // Socket interface overrides
  void setTimeout(int timeoutMs) override;
  void wrapFd(int fd) override;
  int getFd() override;
  size_t read(uint8_t* buf, size_t len) override;
  size_t write(const uint8_t* buf, size_t len) override;
  void bind(const std::string& address, uint16_t port) override;
  void setBlocking(bool blocking) override;
  int poll(int events, int timeoutMs = 0) override;
  bool isOpen() const override;
  void close() override;
  std::string getLocalAddress() const override;
  std::string getRemoteAddress() const override;

  // Callbacks passed to MbedTLS bio functions
  static int mbedtlsSend(void* ctx, const unsigned char* buf, size_t len);
  static int mbedtlsReceive(void* ctx, unsigned char* buf, size_t len);
  static int mbedtlsReceiveTimeout(void* ctx, unsigned char* buf, size_t len,
                                   uint32_t timeoutMs);

 private:
  const char* LOG_TAG = "TLSSocket";

  // MbedTLS structures
  mbedtls_entropy_context entropyCtx{};
  mbedtls_ctr_drbg_context ctrDrbgCtx{};
  mbedtls_ssl_context sslCtx{};
  mbedtls_ssl_config sslConf{};

  void setOptionImpl(int level, int optionName, const void* optionValue,
                     socklen_t optionLen);

 protected:
  std::shared_ptr<bell::TCPSocket> innerSocket;
};
}  // namespace bell::net

namespace bell {
using TLSSocket = net::TLSSocket;
}
