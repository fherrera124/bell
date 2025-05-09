#include "bell/net/TLSSocket.h"

// Standard includes
#include <cerrno>
#include <stdexcept>
#include <system_error>

// MbedTLS includes
#include "mbedtls/error.h"

#include "bell/Logger.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

using namespace bell;

namespace {
// Personalization string used to seed the entropy context
const char* socketPers = "bell-tls";

// Transform MbedTLS error codes to std::error_code, while attempting to map the errors to common errc values
std::error_code mbedtlsToCommonErrc(int mbedtlsErr) {
  switch (mbedtlsErr) {
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
    case MBEDTLS_ERR_NET_CONN_RESET:
      return std::make_error_code(std::errc::connection_reset);
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
      return std::make_error_code(std::errc::operation_would_block);
    case MBEDTLS_ERR_NET_RECV_FAILED:
    case MBEDTLS_ERR_NET_SEND_FAILED:
      return std::make_error_code(std::errc::io_error);
    default:
      return net::make_tls_error_code(mbedtlsErr);
  }
}

// Maps the TCP socket result values to MbedTLS BIO result values
Result<int> makeBioResult(Result<size_t> res, bool reading) {
  if (res) {
    return {static_cast<int>(res.getValue())};
  }

  auto err = res.getError();

  if (err == std::errc::broken_pipe || err == std::errc::connection_reset) {
    return net::make_tls_error_code(MBEDTLS_ERR_NET_CONN_RESET);
  }

  if (err == std::errc::operation_would_block ||
      err == std::errc::interrupted || err == std::errc::timed_out) {
    return reading ? Result<int>(MBEDTLS_ERR_SSL_WANT_READ)
                   : Result<int>(MBEDTLS_ERR_SSL_WANT_WRITE);
  }

  return reading ? Result<int>(MBEDTLS_ERR_NET_RECV_FAILED)
                 : Result<int>(MBEDTLS_ERR_NET_SEND_FAILED);
}
}  // namespace

net::TLSSocket::~TLSSocket() {
  close();

  // Free the MbedTLS structures
  mbedtls_ssl_free(&sslCtx);
  mbedtls_ssl_config_free(&sslConf);
  mbedtls_ctr_drbg_free(&ctrDrbgCtx);
  mbedtls_entropy_free(&entropyCtx);
}

net::TLSSocket::TLSSocket() {
  // Initialize the MbedTLS structures
  mbedtls_entropy_init(&entropyCtx);
  mbedtls_ctr_drbg_init(&ctrDrbgCtx);
  mbedtls_ssl_init(&sslCtx);
  mbedtls_ssl_config_init(&sslConf);

  // TODO: Attach a bundle here

  // Seed the ctr_drbg context
  int ret = mbedtls_ctr_drbg_seed(
      &ctrDrbgCtx, mbedtls_entropy_func, &entropyCtx,
      reinterpret_cast<const uint8_t*>(socketPers), std::strlen(socketPers));

  if (ret != 0) {
    auto err = make_tls_error_code(ret);
    throw std::system_error(err);
  }
}

std::error_code net::TLSSocket::lastError() const {
  return innerSocket.lastError();
}

Result<> net::TLSSocket::connect(const std::string& host, uint16_t port,
                                 int timeoutMs) {
  auto res = innerSocket.connect(host, port, timeoutMs);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to connect to {}: {}", host,
             res.getError().message());
    return res;
  }

  setBlocking(timeoutMs > 0);

  int ret = mbedtls_ssl_config_defaults(&sslConf, MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    return Result<>::fromError(make_tls_error_code(ret));
  }
  // TODO: Bundle verification & TLS 1.3
  mbedtls_ssl_conf_authmode(&sslConf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_max_tls_version(&sslConf, MBEDTLS_SSL_VERSION_TLS1_2);

  mbedtls_ssl_conf_rng(&sslConf, mbedtls_ctr_drbg_random, &ctrDrbgCtx);
  ret = mbedtls_ssl_setup(&sslCtx, &sslConf);
  if (ret != 0) {
    return Result<>::fromError(make_tls_error_code(ret));
  }

  ret = mbedtls_ssl_set_hostname(&sslCtx, host.c_str());
  if (ret != 0) {
    return Result<>::fromError(make_tls_error_code(ret));
  }

  while ((ret = mbedtls_ssl_handshake(&sslCtx)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      BELL_LOG(error, LOG_TAG, "Failed to perform TLS handshake");
      return Result<>::fromError(make_tls_error_code(ret));
    }
  }

  return {};
}

void net::TLSSocket::setupBioCallbacks(bool blocking) {
  mbedtls_ssl_send_t* sendFunc = [](void* ctx, const unsigned char* buf,
                                    size_t len) {
    auto* socket = static_cast<TCPSocket*>(ctx);

    auto res = makeBioResult(socket->write(buf, len), false);
    if (res) {
      return res.getValue();
    }

    return res.getError().value();
  };

  mbedtls_ssl_recv_t* recvFunc = nullptr;
  mbedtls_ssl_recv_timeout_t* recvTimeoutFunc = nullptr;

  if (blocking) {
    recvTimeoutFunc = [](void* ctx, unsigned char* buf, size_t len,
                         uint32_t timeoutMs) {
      auto* socket = static_cast<TCPSocket*>(ctx);

      auto timeoutRes = socket->setReceiveTimeout(timeoutMs);
      if (!timeoutRes) {
        return timeoutRes.getError().value();
      }

      auto res = makeBioResult(socket->read(buf, len), true);
      if (res) {
        return res.getValue();
      }

      return res.getError().value();
    };
  } else {

    recvFunc = [](void* ctx, unsigned char* buf, size_t len) {
      auto* socket = static_cast<TCPSocket*>(ctx);

      auto res = makeBioResult(socket->read(buf, len), true);
      if (res) {
        return res.getValue();
      }

      return res.getError().value();
    };
  }

  mbedtls_ssl_set_bio(&sslCtx, &innerSocket, sendFunc, recvFunc,
                      recvTimeoutFunc);
}

Result<> net::TLSSocket::setReceiveTimeout(int timeoutMs) {
  return innerSocket.setReceiveTimeout(timeoutMs);
};

Result<> net::TLSSocket::setSendTimeout(int timeoutMs) {
  return innerSocket.setSendTimeout(timeoutMs);
};

Result<int> net::TLSSocket::getReceiveTimeout() {
  return innerSocket.getReceiveTimeout();
};

Result<int> net::TLSSocket::getSendTimeout() {
  return innerSocket.getSendTimeout();
};

Result<> net::TLSSocket::setBlocking(bool blocking) {
  setupBioCallbacks(blocking);
  return innerSocket.setBlocking(blocking);
}

Result<bool> net::TLSSocket::getBlocking() const {
  return innerSocket.getBlocking();
}

int net::TLSSocket::getFd() const {
  return innerSocket.getFd();
}

int net::TLSSocket::takeFd() {
  return innerSocket.takeFd();
}

Result<size_t> net::TLSSocket::read(uint8_t* buf, size_t len) {
  int res = mbedtls_ssl_read(&sslCtx, buf, len);

  if (res < 0) {
    return Result<size_t>::fromError(mbedtlsToCommonErrc(res));
  }

  return static_cast<size_t>(res);
}

Result<size_t> net::TLSSocket::write(const uint8_t* buf, size_t len) {
  int res = mbedtls_ssl_write(&sslCtx, buf, len);

  if (res < 0) {
    return Result<size_t>::fromError(mbedtlsToCommonErrc(res));
  }

  return static_cast<size_t>(res);
}

bool net::TLSSocket::isValid() const {
  return innerSocket.isValid();
}

void net::TLSSocket::close() {
  if (innerSocket.isValid()) {
    mbedtls_ssl_close_notify(&sslCtx);
    innerSocket.close();
  }
}
