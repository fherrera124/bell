// #include "bell/net/TLSSocket.h"

// // Standard includes
// #include <cerrno>
// #include <stdexcept>

// // MbedTLS includes
// #include "mbedtls/error.h"

// #include "bell/Logger.h"

// using namespace bell;

// namespace {
// // Personalization string used to seed the entropy context
// const char* socketPers = "bell-tls";

// // Convert a MbedTLS error code to a human-readable string
// std::string mbedtlsErrString(int error) {
//   std::string result(128, '\0');
//   mbedtls_strerror(error, result.data(), result.size());

//   result.resize(std::strlen(result.c_str()));

//   return result;
// }
// }  // namespace

// net::TLSSocket::~TLSSocket() {
//   close();

//   // Free the MbedTLS structures
//   mbedtls_ssl_free(&sslCtx);
//   mbedtls_ssl_config_free(&sslConf);
//   mbedtls_ctr_drbg_free(&ctrDrbgCtx);
//   mbedtls_entropy_free(&entropyCtx);
// }

// net::TLSSocket::TLSSocket() {
//   // Initialize the MbedTLS structures
//   mbedtls_entropy_init(&entropyCtx);
//   mbedtls_ctr_drbg_init(&ctrDrbgCtx);
//   mbedtls_ssl_init(&sslCtx);
//   mbedtls_ssl_config_init(&sslConf);

//   // TODO: Attach a bundle here

//   // Seed the ctr_drbg context
//   int ret = mbedtls_ctr_drbg_seed(
//       &ctrDrbgCtx, mbedtls_entropy_func, &entropyCtx,
//       reinterpret_cast<const uint8_t*>(socketPers), std::strlen(socketPers));

//   if (ret != 0) {
//     BELL_LOG(error, LOG_TAG, "Failed to seed the ctr_drbg context: {}",
//              mbedtlsErrString(ret));
//     throw std::runtime_error("Failed to seed the ctr_drbg context");
//   }
// }

// net::Result<> net::TLSSocket::connect(const std::string& host, uint16_t port,
//                                       int timeoutMs) {
//   innerSocket = std::make_shared<TCPSocket>();  // Create the inner socket

//   // Connect the inner socket
//   innerSocket->connect(host, port, timeoutMs);

//   int ret = mbedtls_ssl_config_defaults(&sslConf, MBEDTLS_SSL_IS_CLIENT,
//                                         MBEDTLS_SSL_TRANSPORT_STREAM,
//                                         MBEDTLS_SSL_PRESET_DEFAULT);
//   if (ret != 0) {
//     BELL_LOG(error, LOG_TAG, "Failed to set the SSL config defaults: {}",
//              mbedtlsErrString(ret));
//     throw std::runtime_error("Failed to set the SSL config defaults");
//   }

//   // TODO: Bundle verification & TLS 1.3
//   mbedtls_ssl_conf_authmode(&sslConf, MBEDTLS_SSL_VERIFY_NONE);
//   mbedtls_ssl_conf_max_tls_version(&sslConf, MBEDTLS_SSL_VERSION_TLS1_2);

//   mbedtls_ssl_conf_rng(&sslConf, mbedtls_ctr_drbg_random, &ctrDrbgCtx);
//   ret = mbedtls_ssl_setup(&sslCtx, &sslConf);
//   if (ret != 0) {
//     BELL_LOG(error, LOG_TAG, "Failed to set up the SSL context: {}", ret);
//     throw std::runtime_error("Failed to set up the SSL context");
//   }

//   ret = mbedtls_ssl_set_hostname(&sslCtx, host.c_str());
//   if (ret != 0) {
//     BELL_LOG(error, LOG_TAG, "Failed to set the hostname: {}",
//              mbedtlsErrString(ret));
//     throw std::runtime_error("Failed to set the hostname");
//   }

//   while ((ret = mbedtls_ssl_handshake(&sslCtx)) != 0) {
//     if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
//       BELL_LOG(error, LOG_TAG, "failed! config returned {}\n",
//                mbedtlsErrString(ret));
//       throw std::runtime_error("mbedtls_ssl_handshake error");
//     }
//   }

//   return {};
// }

// void net::TLSSocket::setReceiveTimeout(int timeoutMs) {
//   if (!innerSocket) {
//     throw std::runtime_error("Socket is not connected");
//   }

//   if (timeoutMs > 0) {
//     mbedtls_ssl_set_bio(&sslCtx, innerSocket.get(), mbedtlsSend, nullptr,
//                         mbedtlsReceiveTimeout);
//   } else {
//     mbedtls_ssl_set_bio(&sslCtx, innerSocket.get(), mbedtlsSend, mbedtlsReceive,
//                         nullptr);
//   }
//   innerSocket->setReceiveTimeout(timeoutMs);
// };

// void net::TLSSocket::setSendTimeout(int timeoutMs) {
//   if (!innerSocket) {
//     throw std::runtime_error("Socket is not connected");
//   }

//   innerSocket->setSendTimeout(timeoutMs);
// };

// int net::TLSSocket::getFd() {
//   if (!innerSocket) {
//     throw std::runtime_error("Socket is not connected");
//   }
//   return innerSocket->getFd();
// }

// net::Result<size_t> net::TLSSocket::read(uint8_t* buf, size_t len) {
//   if (!innerSocket) {
//     throw std::runtime_error("Socket is not connected");
//   }

//   int ret = mbedtls_ssl_read(&sslCtx, buf, len);

//   if ((ret == MBEDTLS_ERR_SSL_WANT_READ) ||
//       (ret == MBEDTLS_ERR_SSL_WANT_WRITE)) {
//     // Timeout, ignore
//     return 0;
//   }

//   if (ret < 0) {
//     throw std::runtime_error(fmt::format(
//         "Failed to read from the TLS socket, err={}", mbedtlsErrString(ret)));
//   }

//   return ret;
// }

// net::Result<size_t> net::TLSSocket::write(const uint8_t* buf, size_t len) {
//   if (!innerSocket) {
//     throw std::runtime_error("Socket is not connected");
//   }

//   int ret = mbedtls_ssl_write(&sslCtx, buf, len);

//   if ((ret == MBEDTLS_ERR_SSL_WANT_READ) ||
//       (ret == MBEDTLS_ERR_SSL_WANT_WRITE)) {
//     // Timeout, ignore
//     return 0;
//   }

//   if (ret < 0) {
//     throw std::runtime_error(
//         fmt::format("Failed to write to the TLS socket, err={}", ret));
//   }

//   return ret;
// }

// net::Result<> net::TLSSocket::bind(const std::string& /*address*/,
//                                    uint16_t /*port*/) {
//   throw std::runtime_error("Not implemented");
// }

// void net::TLSSocket::setBlocking(bool blocking) {
//   if (!innerSocket) {
//     throw std::runtime_error("Socket is not connected");
//   }
//   innerSocket->setBlocking(blocking);
// };

// bool net::TLSSocket::isValid() const {
//   return innerSocket && innerSocket->isValid();
// }

// void net::TLSSocket::close() {
//   if (innerSocket && innerSocket->isValid()) {
//     mbedtls_ssl_close_notify(&sslCtx);
//     innerSocket->close();
//   }
// }

// int net::TLSSocket::mbedtlsReceive(void* ctx, unsigned char* buf, size_t len) {
//   auto* socket = static_cast<TCPSocket*>(ctx);

//   try {
//     return socket->read(buf, len).unwrap();
//   } catch (...) {
//     // Handle the error
//     auto err = errno;
//     switch (err) {
//       case EWOULDBLOCK:
//       case EINTR:
//         return MBEDTLS_ERR_SSL_WANT_READ;
//       default:
//         return err;
//     }
//   }
// }

// int net::TLSSocket::mbedtlsSend(void* ctx, const unsigned char* buf,
//                                 size_t len) {
//   auto* socket = static_cast<TCPSocket*>(ctx);
//   try {
//     return socket->write(buf, len).unwrap();
//   } catch (...) {
//     int err = errno;
//     // Handle the error
//     switch (err) {
//       case EWOULDBLOCK:
//       case EINTR:
//         return MBEDTLS_ERR_SSL_WANT_WRITE;
//       default:
//         return err;
//     }
//   }
// }

// int net::TLSSocket::mbedtlsReceiveTimeout(void* ctx, unsigned char* buf,
//                                           size_t len, uint32_t /*timeoutMs*/) {
//   auto* socket = static_cast<TCPSocket*>(ctx);

//   try {
//     return socket->read(buf, len).unwrap();
//   } catch (...) {
//     // Handle the error
//     auto err = errno;
//     switch (err) {
//       case EWOULDBLOCK:
//       case EINTR:
//         return MBEDTLS_ERR_SSL_WANT_READ;
//       default:
//         return err;
//     }
//   }
// }