#include "bell/net/POSIXSocket.h"

#include <fmt/format.h>
#include <cerrno>
#include <stdexcept>

#include "bell/Logger.h"
#include "bell/net/IpAddress.h"
#include "bell/utils/Utils.h"

// Platform specific socket includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "win32shim.h"
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __sun
#include <sys/filio.h>
#endif
#endif

#include <fcntl.h>
#include <sys/poll.h>

using namespace bell::net;

void POSIXSocket::setBlocking(bool blocking) {
  isBlocking = blocking;

  if (isOpen()) {
#ifdef _WIN32
    unsigned long mode = blocking ? 0 : 1;
    if (ioctlsocket(sockFd, FIONBIO, &mode) != 0) {
      throw std::runtime_error("Could not set socket flags");
    }
#else
    int flags = fcntl(sockFd, F_GETFL, 0);
    if (flags == -1) {
      throw std::runtime_error("Could not get socket flags");
    }
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(sockFd, F_SETFL, flags) != 0) {
      throw std::runtime_error("Could not set socket flags");
    }
#endif
  }
}

void POSIXSocket::wrapFd(int fd) {
  if (fd != -1) {
    sockFd = fd;
    isClosed = false;
  }
}

std::error_code POSIXSocket::lastError() const {
  if (!isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  int err = errno;

  return {err, std::system_category()};
}

Result<size_t> POSIXSocket::read(uint8_t* buf, size_t len) {
  if (!isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Perform the actual read operation
  ssize_t res = recv(sockFd, buf, len, 0);
  if (res < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      close();
    }

    return Result<size_t>::fromLastErrno();
  }

  return static_cast<size_t>(res);
}

Result<size_t> POSIXSocket::write(const uint8_t* buf, size_t len) {
  if (!isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  // Perform the actual write operation
  ssize_t res = ::send(sockFd, buf, len, MSG_NOSIGNAL);
  if (res < 0) {
    BELL_LOG(error, "POSIXSocket", "Error in send: {}", strerror(errno));
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      close();
    }
    return Result<size_t>::fromLastErrno();
  }

  return {static_cast<size_t>(res)};
}

void POSIXSocket::createFd(int domain, int protocol) {
  sockFd = socket(domain, sockType, protocol);
  if (sockFd < 0) {
    throw std::runtime_error(fmt::format("Could not create socket {}", errno));
  }
  isClosed = false;
}

int POSIXSocket::getFd() {
  return sockFd;
}

bool POSIXSocket::isOpen() const {
  return !isClosed;
}

void POSIXSocket::close() {
  if (isOpen()) {
#ifdef _WIN32
    closesocket(sockFd);
#else
    ::close(sockFd);
#endif
    isClosed = true;
  }
}

Result<> POSIXSocket::bind(const std::string& address, uint16_t port) {
  if (isOpen()) {
    close();
  }

  auto res = IpAddress::resolveDomain(address, sockType);

  if (!res) {
    return res.getError();
  }

  auto resolved = res.getValue();
  resolved.setPort(port);

  createFd(resolved.getFamily(), IPPROTO_IP);
  isClosed = false;

  // Set REUSEADDR option
  setOption(SOL_SOCKET, SO_REUSEADDR, 1);

  if (::bind(sockFd, resolved.getSockAddrPtr(), resolved.getSockAddrLen()) !=
      0) {
    isClosed = true;
    return Result<>::fromLastErrno();
  }

  return {};
}

void POSIXSocket::setOptionImpl(int level, int optionName,
                                const void* optionValue, socklen_t optionLen) {
  if (!isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  if (setsockopt(getFd(), level, optionName, optionValue, optionLen) == -1) {
    throw std::runtime_error(
        fmt::format("Failed to set socket option: {}", strerror(errno)));
  }
}

void POSIXSocket::setReceiveTimeout(int timeoutMs) {
  if (timeoutMs == 0) {
    // Switch to blocking mode
    setBlocking(true);
    return;
  }

  setBlocking(false);
  auto timeVal = bell::utils::millisecondsToTimeval(timeoutMs);
  this->timeoutMs = timeoutMs;

  setOption(SOL_SOCKET, SO_RCVTIMEO, timeVal);
}

void POSIXSocket::setSendTimeout(int timeoutMs) {
  if (timeoutMs == 0) {
    // Switch to blocking mode
    setBlocking(true);
    return;
  }

  setBlocking(false);
  auto timeVal = bell::utils::millisecondsToTimeval(timeoutMs);
  this->timeoutMs = timeoutMs;

  setOption(SOL_SOCKET, SO_SNDTIMEO, timeVal);
}
