#include "bell/net/POSIXSocket.h"

#include <fmt/format.h>
#include <cerrno>

#include "bell/Logger.h"
#include "bell/net/IpAddress.h"

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
#include <poll.h>

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

int POSIXSocket::poll(int events, int timeoutMs) {
  if (!isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  struct pollfd pfd {};
  pfd.fd = sockFd;
  pfd.events = static_cast<short>(events);

  int result = ::poll(&pfd, 1, timeoutMs);
  if (result < 0) {
    throw std::runtime_error("Poll failed");
  }

  return result > 0 ? pfd.revents
                    : 0;  // Return the events that occurred or 0 if timeout.
}

size_t POSIXSocket::read(uint8_t* buf, size_t len, int timeoutMs) {
  if (timeoutMs > 0) {
    // Use the socket's poll method to wait for data to be readable.
    int events = poll(POLLIN, timeoutMs);
    if (!(events & POLLIN)) {
      // Timeout occurred, or no readable events.
      throw std::runtime_error("Socket read timed out or no data available");
    }
  }

  // Perform the actual read operation
  ssize_t res = recv(sockFd, buf, len, 0);
  if (res < 0) {
    close();
    throw std::runtime_error(fmt::format("Error in recv, {}", strerror(errno)));
  }

  return static_cast<size_t>(res);
}

size_t POSIXSocket::write(const uint8_t* buf, size_t len, int timeoutMs) {
  if (timeoutMs > 0) {
    // Use the socket's poll method to wait for the socket to become writable.
    int events = poll(POLLOUT, timeoutMs);
    if (!(events & POLLOUT)) {
      // Timeout occurred, or no writable events.
      throw std::runtime_error("Socket write timed out or socket not writable");
    }
  }

  // Perform the actual write operation
  ssize_t res = ::send(sockFd, buf, len, 0);
  if (res < 0) {
    close();
    throw std::runtime_error("Error in send");
  }

  return static_cast<size_t>(res);
}

int POSIXSocket::getFd() {
  return sockFd;
}

bool POSIXSocket::isOpen() {
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

void POSIXSocket::listen(int backlog) {
  if (!isOpen()) {
    throw std::runtime_error("Socket is not open");
  }

  if (::listen(sockFd, backlog) != 0) {
    throw std::runtime_error("Listen failed");
  }

  isListening = true;
  BELL_LOG(info, LOG_TAG, "Listening on socket {}", sockFd);
}

void POSIXSocket::bind(const std::string& address, uint16_t port) {
  if (isOpen()) {
    close();
  }

  IpAddress resolved = IpAddress::resolveDomain(address, SOCK_STREAM);
  resolved.setPort(port);

  sockFd = socket(resolved.getFamily(), SOCK_STREAM, IPPROTO_IP);
  if (sockFd < 0) {
    throw std::runtime_error("Could not create socket " +
                             std::string(strerror(errno)));
  }

  if (::bind(sockFd, resolved.getSockAddrPtr(), resolved.getSockAddrLen()) !=
      0) {
    throw std::runtime_error(fmt::format("Bind failed: {}", strerror(errno)));
  }

  isClosed = false;
  BELL_LOG(info, LOG_TAG, "Bound to {}:{}", address.c_str(), port);
}

std::string POSIXSocket::getLocalAddress() const {
  // TODO:
  return "";
};

std::string POSIXSocket::getRemoteAddress() const {
  return "";
};
