#include "bell/net/POSIXSocket.h"

#include <fmt/format.h>
#include "bell/Result.h"
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

bell::Result<> POSIXSocket::setBlocking(bool blocking) {
  if (isValid()) {
#ifdef _WIN32
    unsigned long mode = blocking ? 0 : 1;
    if (ioctlsocket(sockFd, FIONBIO, &mode) != 0) {
      // throw std::runtime_error("Could not set socket flags");
      return Result<>::fromLastErrno();
    }
#else
    int flags = fcntl(sockFd, F_GETFL, 0);
    if (flags == -1) {
      throw std::runtime_error("Could not get socket flags");
    }
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(sockFd, F_SETFL, flags) != 0) {
      return Result<>::fromLastErrno();
    }
#endif
  }

  return {};
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

bell::Result<size_t> POSIXSocket::read(uint8_t* buf, size_t len) {
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

bell::Result<size_t> POSIXSocket::write(const uint8_t* buf, size_t len) {
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

bell::Result<> POSIXSocket::createFd(int domain, int protocol) {
  this->sockFd = socket(domain, getSockType(), protocol);
  if (sockFd < 0) {
    return Result<>::fromLastErrno();
  }

  return {};
}

int POSIXSocket::getFd() const {
  return sockFd;
}

int POSIXSocket::takeFd() {
  int fd = sockFd;
  sockFd = INVALID_FD;
  return fd;
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

bell::Result<int> POSIXSocket::bind(const std::string& address, uint16_t port,
                                    bool reuseAddr) {
  auto res = IpAddress::resolveDomain(address, getSockType());

  if (!res) {
    return Result<int>::fromError(res.getError());
  }

  auto resolved = res.getValue();
  resolved.setPort(port);

  auto fdRes = createFd(resolved.getFamily(), IPPROTO_IP);

  if (!fdRes) {
    return Result<int>::fromError(fdRes.getError());
  }

  if (reuseAddr) {
    auto optionRes = setOption(SOL_SOCKET, SO_REUSEADDR, 1);

    if (!optionRes) {
      return Result<int>::fromError(optionRes.getError());
    }
  }

  if (::bind(sockFd, resolved.getSockAddrPtrConst(),
             resolved.getSockAddrLen()) != 0) {
    close();
    return Result<int>::fromLastErrno();
  }

  socklen_t servSockLen = resolved.getSockAddrLen();

  // Retrieve assigned port
  if (getsockname(sockFd, resolved.getSockAddrPtr(), &servSockLen) != 0) {
    close();
    return Result<int>::fromLastErrno();
  }

  if (resolved.getPort().has_value()) {
    return resolved.getPort().value();
  }

  return Result<int>::fromError(std::errc::invalid_argument);
}

bell::Result<> POSIXSocket::setOptionImpl(int level, int optionName,
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

bell::Result<> POSIXSocket::getOptionImpl(int level, int optionName,
                                          void* optionValue,
                                          socklen_t optionLen) {
  if (!isValid()) {
    return Result<>::fromError(std::errc::invalid_argument);
  }

  if (getsockopt(getFd(), level, optionName, optionValue, &optionLen) == -1) {
    return Result<>::fromLastErrno();
  }

  return {};
}

bell::Result<> POSIXSocket::setReceiveTimeout(int timeoutMs) {
  auto timeVal = bell::utils::millisecondsToTimeval(timeoutMs);
  return setOption(SOL_SOCKET, SO_RCVTIMEO, timeVal);
}

bell::Result<> POSIXSocket::setSendTimeout(int timeoutMs) {
  auto timeVal = bell::utils::millisecondsToTimeval(timeoutMs);
  return setOption(SOL_SOCKET, SO_SNDTIMEO, timeVal);
}

bell::Result<int> POSIXSocket::getReceiveTimeout() {
  struct timeval timeVal {};
  auto res = getOptionImpl(SOL_SOCKET, SO_RCVTIMEO, &timeVal, sizeof(timeVal));

  if (!res) {
    return Result<int>::fromError(res.getError());
  }

  return static_cast<int>(utils::timevalToMilliseconds(timeVal));
}

bell::Result<int> POSIXSocket::getSendTimeout() {
  struct timeval timeVal {};
  auto res = getOptionImpl(SOL_SOCKET, SO_SNDTIMEO, &timeVal, sizeof(timeVal));

  if (!res) {
    return Result<int>::fromError(res.getError());
  }

  return static_cast<int>(utils::timevalToMilliseconds(timeVal));
}

bell::Result<bool> POSIXSocket::getBlocking() const {
  if (!isValid()) {
    return Result<bool>::fromError(std::errc::invalid_argument);
  }

  int flags = fcntl(sockFd, F_GETFL, 0);
  if (flags == -1) {
    return Result<bool>::fromLastErrno();
  }

  return !(flags & O_NONBLOCK);
}
