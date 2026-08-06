#include "bell/net/TLSSocket.h"
#include <netinet/tcp.h>

// Standard includes
#include <cerrno>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <system_error>

// MbedTLS includes
#include "mbedtls/error.h"

#include "bell/Logger.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "nonstd/expected.hpp"

using namespace bell;

namespace {
#if MBEDTLS_VERSION_MAJOR < 4
// Personalization string used to seed the entropy context
const char* socketPers = "bell-tls";
#endif

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
bell::Result<int> transformBioRes(bell::Result<size_t> res, bool reading) {
  if (res) {
    return {static_cast<int>(*res)};
  }

  if (res.error() == std::errc::broken_pipe ||
      res.error() == std::errc::connection_reset) {
    return nonstd::make_unexpected(
        net::make_tls_error_code(MBEDTLS_ERR_NET_CONN_RESET));
  }

  if (res.error() == std::errc::operation_would_block ||
      res.error() == std::errc::interrupted ||
      res.error() == std::errc::timed_out) {
    return reading ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  return reading ? MBEDTLS_ERR_NET_RECV_FAILED : MBEDTLS_ERR_NET_SEND_FAILED;
}
}  // namespace

net::TLSSocket::~TLSSocket() {
  close();

  // Free the MbedTLS structures
  mbedtls_ssl_free(&sslCtx);
  mbedtls_ssl_config_free(&sslConf);
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_ctr_drbg_free(&ctrDrbgCtx);
  mbedtls_entropy_free(&entropyCtx);
#endif
}

net::TLSSocket::TLSSocket() {
  // Initialize the MbedTLS structures
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_entropy_init(&entropyCtx);
  mbedtls_ctr_drbg_init(&ctrDrbgCtx);
#endif
  mbedtls_ssl_init(&sslCtx);
  mbedtls_ssl_config_init(&sslConf);

  // TODO: Attach a bundle here

#if MBEDTLS_VERSION_MAJOR < 4
  // Seed the ctr_drbg context
  int ret = mbedtls_ctr_drbg_seed(
      &ctrDrbgCtx, mbedtls_entropy_func, &entropyCtx,
      reinterpret_cast<const uint8_t*>(socketPers), std::strlen(socketPers));

  if (ret != 0) {
    auto err = make_tls_error_code(ret);
    throw std::system_error(err);
  }
#else
  // Mbed TLS 4.0 draws randomness from the PSA subsystem, which must be
  // initialized before the handshake. psa_crypto_init() is idempotent; ESP-IDF
  // seeds PSA during boot, but host builds rely on this call.
  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    throw std::system_error(make_tls_error_code(status));
  }
#endif
}

std::error_code net::TLSSocket::lastError() const {
  return innerSocket.lastError();
}

bell::Result<> net::TLSSocket::connect(const std::string& host, uint16_t port,
                                       int timeoutMs) {
  auto res = innerSocket.connect(host, port, timeoutMs);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to connect to {}: {}", host, res.error());
    return res;
  }

  auto setBlockingRes = setBlocking(timeoutMs > 0);
  if (!setBlockingRes) {
    return setBlockingRes;
  }

  int ret = mbedtls_ssl_config_defaults(&sslConf, MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    return nonstd::make_unexpected(make_tls_error_code(ret));
  }
  // TODO: Bundle verification & TLS 1.3
  mbedtls_ssl_conf_authmode(&sslConf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_max_tls_version(&sslConf, MBEDTLS_SSL_VERSION_TLS1_2);

  // Bounds every blocking read/write on this connection, including the
  // handshake below - which runs before any caller gets a chance to call
  // setReceiveTimeout()/setSendTimeout() itself. Without this, a
  // blocking-mode read that never gets a response (peer stalls mid-response,
  // a CDN edge that stops responding, etc.) blocks forever - a real hardware
  // hang reproduced via AudioDecoderImpl::openStream()'s CDN fetch (uses
  // operationTimeoutMs=3000, i.e. blocking mode): it ran synchronously on
  // EventLoop's own dispatch task, so the hang didn't just stall this one
  // request, it permanently froze all future dealer/queue processing too,
  // with only the separate poller task's own WS ping/pong still visible in
  // the log.
  //
  // Applied directly to the socket rather than via mbedTLS's own
  // mbedtls_ssl_conf_read_timeout()/f_recv_timeout mechanism, which this
  // class used to rely on: that config is set once, here, and never revisited
  // for the lifetime of this TLSSocket, so on a pooled/reused connection
  // (DefaultTransport::execute()) every read after the first silently kept
  // using this connect() call's timeoutMs instead of the current request's -
  // verified against mbedTLS's own ssl_msg.c that for TLS-over-TCP
  // (MBEDTLS_SSL_TRANSPORT_STREAM, what this class uses - not DTLS),
  // f_recv_timeout adds no behavior beyond handing its timeout argument
  // through to the callback, so a plain f_recv-style callback (see
  // setupBioCallbacks() below) reading an already-timeout-bound blocking
  // socket is equivalent, and keeps the socket's own, always-current
  // SO_RCVTIMEO/SO_SNDTIMEO as the single source of truth.
  if (timeoutMs > 0) {
    (void)innerSocket.setReceiveTimeout(timeoutMs);
    (void)innerSocket.setSendTimeout(timeoutMs);
  }

  // mbedTLS 4.0+ doesn't declare mbedtls_ssl_conf_rng() at all - the SSL
  // layer draws randomness from PSA unconditionally there instead (see
  // ensurePsaCryptoInit() above). mbedTLS <4.0 requires this explicitly
  // ("RNG function (mandatory)" per its own doc comment).
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_ssl_conf_rng(&sslConf, mbedtls_ctr_drbg_random, &ctrDrbgCtx);
#endif
  ret = mbedtls_ssl_setup(&sslCtx, &sslConf);
  if (ret != 0) {
    return nonstd::make_unexpected(make_tls_error_code(ret));
  }

  // Wires the BIO once per connection - unlike the old recvTimeoutFunc
  // variant, the plain callbacks set up here don't depend on timeoutMs/
  // blocking mode, so this doesn't need re-running when setBlocking() is
  // called later (e.g. SocketBuffer forcing blocking mode on a pooled
  // connection).
  setupBioCallbacks();

  ret = mbedtls_ssl_set_hostname(&sslCtx, host.c_str());
  if (ret != 0) {
    return nonstd::make_unexpected(make_tls_error_code(ret));
  }

  // mbedTLS puts no ceiling on how many times this loop may retry a
  // WANT_READ/WANT_WRITE for TLS-over-TCP (that retry-limiting logic is
  // DTLS-only) - a peer that goes silent mid-handshake without closing the
  // connection would otherwise retry forever. Tracks a deadline across the
  // whole loop instead of just each read/write, same as ESP-IDF's esp-tls
  // (esp_tls_conn_new_sync()).
  auto handshakeDeadline =
      timeoutMs > 0 ? std::optional{std::chrono::steady_clock::now() +
                                    std::chrono::milliseconds(timeoutMs)}
                    : std::nullopt;

  while ((ret = mbedtls_ssl_handshake(&sslCtx)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      BELL_LOG(error, LOG_TAG, "Failed to perform TLS handshake {}", ret);
      return nonstd::make_unexpected(make_tls_error_code(ret));
    }
    if (handshakeDeadline &&
        std::chrono::steady_clock::now() >= *handshakeDeadline) {
      BELL_LOG(error, LOG_TAG, "TLS handshake timed out");
      return bell::make_unexpected_errc(std::errc::timed_out);
    }
  }

  return {};
}

void net::TLSSocket::setupBioCallbacks() {
  mbedtls_ssl_send_t* sendFunc = [](void* ctx, const unsigned char* buf,
                                    size_t len) {
    auto* socket = static_cast<TCPSocket*>(ctx);

    auto res = transformBioRes(
        socket->write(reinterpret_cast<const std::byte*>(buf), len), false);
    if (res) {
      return *res;
    }

    return res.error().value();
  };

  // No f_recv_timeout variant: the socket's own SO_RCVTIMEO (set in
  // connect(), re-applied per request by DefaultTransport::execute() on
  // pooled connections) already bounds this read when blocking, and reports
  // the same operation_would_block/timed_out errc that transformBioRes()
  // below maps to WANT_READ either way - a timeout and a "nothing ready yet"
  // non-blocking read are indistinguishable to mbedTLS here on purpose, same
  // as they were before this connection ever had a bell::net::TLSSocket
  // wrapped around it.
  mbedtls_ssl_recv_t* recvFunc = [](void* ctx, unsigned char* buf,
                                    size_t len) {
    auto* socket = static_cast<TCPSocket*>(ctx);

    auto res = transformBioRes(
        socket->read(reinterpret_cast<std::byte*>(buf), len), true);
    if (res) {
      return *res;
    }

    return res.error().value();
  };

  mbedtls_ssl_set_bio(&sslCtx, &innerSocket, sendFunc, recvFunc, nullptr);
}

bell::Result<> net::TLSSocket::setReceiveTimeout(int timeoutMs) {
  return innerSocket.setReceiveTimeout(timeoutMs);
};

bell::Result<> net::TLSSocket::setSendTimeout(int timeoutMs) {
  return innerSocket.setSendTimeout(timeoutMs);
};

bell::Result<int> net::TLSSocket::getReceiveTimeout() {
  return innerSocket.getReceiveTimeout();
};

bell::Result<int> net::TLSSocket::getSendTimeout() {
  return innerSocket.getSendTimeout();
};

bell::Result<> net::TLSSocket::setBlocking(bool blocking) {
  return innerSocket.setBlocking(blocking);
}

bell::Result<bool> net::TLSSocket::getBlocking() const {
  return innerSocket.getBlocking();
}

int net::TLSSocket::getFd() const {
  return innerSocket.getFd();
}

int net::TLSSocket::takeFd() {
  return innerSocket.takeFd();
}

bell::Result<size_t> net::TLSSocket::read(std::byte* buf, size_t len) {
  int res = mbedtls_ssl_read(&sslCtx, reinterpret_cast<uint8_t*>(buf), len);

  if (res < 0) {
    return nonstd::make_unexpected(mbedtlsToCommonErrc(res));
  }

  return static_cast<size_t>(res);
}

bell::Result<size_t> net::TLSSocket::write(const std::byte* buf, size_t len) {
  int res =
      mbedtls_ssl_write(&sslCtx, reinterpret_cast<const uint8_t*>(buf), len);

  if (res < 0) {
    return nonstd::make_unexpected(mbedtlsToCommonErrc(res));
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
