#include "bell/net/UDPSocket.h"

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

UDPSocket::~UDPSocket() {
  close();
}

size_t UDPSocket::recvfrom(uint8_t* buf, size_t len, const IpAddress& address,
                           int timeoutMs) {
  if (timeoutMs > 0) {
    // Use the socket's poll method to wait for data to be readable.
    int events = poll(POLLIN, timeoutMs);
    if (!(events & POLLIN)) {
      // Timeout occurred, or no readable events.
      throw std::runtime_error("Socket read timed out or no data available");
    }
  }

  socklen_t addressLen = address.getSockAddrLen();

  // Perform the actual read operation
  // Using const_cast to remove the const qualifier from the address, as recvfrom expects a non-const
  // This is quite ugly, but recvfrom is a C API and we can't change it.
  ssize_t res =
      ::recvfrom(sockFd, buf, len, 0,
                 const_cast<sockaddr*>(address.getSockAddrPtr()),  // NOLINT
                 &addressLen);
  if (res < 0) {
    close();
    throw std::runtime_error(fmt::format("Error in recv, {}", errno));
  }

  return static_cast<size_t>(res);
}

size_t UDPSocket::sendto(const uint8_t* buf, size_t len,
                         const IpAddress& address, int timeoutMs) {
  if (timeoutMs > 0) {
    // Use the socket's poll method to wait for the socket to become writable.
    int events = poll(POLLOUT, timeoutMs);
    if (!(events & POLLOUT)) {
      // Timeout occurred, or no writable events.
      throw std::runtime_error("Socket write timed out or socket not writable");
    }
  }

  // Using const_cast to remove the const qualifier from the address, as sendto expects a non-const
  // This is quite ugly, but recvfrom is a C API and we can't change it.
  ssize_t res =
      ::sendto(sockFd, buf, len, 0,
               const_cast<sockaddr*>(address.getSockAddrPtr()),  // NOLINT
               address.getSockAddrLen());
  if (res < 0) {
    close();
    throw std::runtime_error(fmt::format("Error in send, {}", errno));
  }

  return static_cast<size_t>(res);
}
