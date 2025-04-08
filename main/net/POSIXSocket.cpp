#include "bell/net/POSIXSocket.h"

#include <fmt/format.h>
#include <cerrno>
#include <stdexcept>

#include "bell/Logger.h"
#include "bell/net/IpAddress.h"
#include "bell/net/Result.h"
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

  if (isValid()) {
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

std::error_code POSIXSocket::lastError() const {
  int sockErr;
  socklen_t sockErrLen = sizeof(sockErr);
  if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &sockErr, &sockErrLen) < 0 ||
      sockErr != 0) {
    return {sockErr, std::system_category()};
  }

  return {0, std::system_category()};
}

Result<size_t> POSIXSocket::read(uint8_t* buf, size_t len) {
  if (!isValid()) {
    return Result<size_t>::fromError(std::errc::invalid_argument);
  }

  // Perform the actual read operation
  ssize_t res = ::recv(sockFd, buf, len, 0);
  if (res < 0) {
    return Result<size_t>::fromLastErrno();
  }

  return static_cast<size_t>(res);
}

Result<size_t> POSIXSocket::write(const uint8_t* buf, size_t len) {
  if (!isValid()) {
    return Result<size_t>::fromError(std::errc::invalid_argument);
  }

  // Perform the actual write operation
  ssize_t res = ::send(sockFd, buf, len, MSG_NOSIGNAL);
  if (res < 0) {
    return Result<size_t>::fromLastErrno();
  }

  return {static_cast<size_t>(res)};
}

Result<> POSIXSocket::createFd(int domain, int type, int protocol) {
  sockFd = socket(domain, type, protocol);
  if (sockFd < 0) {
    return Result<>::fromLastErrno();
  }

  return {};
}

int POSIXSocket::getFd() {
  return sockFd;
}

bool POSIXSocket::isValid() const {
  return sockFd != INVALID_FD;
}

void POSIXSocket::close() {
  if (isValid()) {
#ifdef _WIN32
    closesocket(sockFd);
#else
    ::close(sockFd);
#endif
    sockFd = INVALID_FD;
  }
}

Result<> POSIXSocket::bind(const std::string& address, uint16_t port) {
  if (!isValid()) {
    return Result<>::fromError(std::errc::invalid_argument);
  }

  auto res = IpAddress::resolveDomain(address, sockType);

  if (!res) {
    return Result<>::fromError(res.getError());
  }

  auto resolved = res.getValue();
  resolved.setPort(port);

  createFd(resolved.getFamily(), IPPROTO_IP);
  isClosed = false;

  // Set REUSEADDR option
  auto optionRes = setOption(SOL_SOCKET, SO_REUSEADDR, 1);

  if (!optionRes) {
    return optionRes;
  }

  if (::bind(sockFd, resolved.getSockAddrPtr(), resolved.getSockAddrLen()) !=
      0) {
    close();
    return Result<>::fromLastErrno();
  }

  return {};
}

Result<> POSIXSocket::setOptionImpl(int level, int optionName,
                                    const void* optionValue,
                                    socklen_t optionLen) {
  if (!isValid()) {
    return Result<>::fromError(std::errc::invalid_argument);
  }

  if (setsockopt(getFd(), level, optionName, optionValue, optionLen) == -1) {
    return Result<>::fromLastErrno();
  }

  return {};
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
