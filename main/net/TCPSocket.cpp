#include "bell/net/TCPSocket.h"

#include "bell/Logger.h"
#include "bell/net/IpAddress.h"
#include "bell/net/Result.h"

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

Result<> TCPSocket::connect(const std::string& host, uint16_t port,
                            int timeoutMs) {
  // Close the socket if it is already open
  if (sockFd != INVALID_FD) {
    BELL_LOG(debug, LOG_TAG, "Socket already open");
    return Result<>::fromError(std::errc::invalid_argument);
  }

  auto resolveRes = IpAddress::resolveDomain(host, SOCK_STREAM);

  if (!resolveRes) {
    BELL_LOG(error, LOG_TAG, "Could not resolve {}. Error {}", host.c_str(),
             resolveRes.getError().message());
    return Result<>::fromError(resolveRes.getError());
  }

  auto destinationAddress = resolveRes.getValue();
  destinationAddress.setPort(port);

  auto res = createFd(destinationAddress.getFamily(), IPPROTO_IP);

  if (!res) {
    BELL_LOG(error, LOG_TAG, "Could not create socket. Error {}",
             res.errorMessage());
    return res;
  }

  auto isBlockingRes = getBlocking();

  if (!isBlockingRes) {
    BELL_LOG(error, LOG_TAG, "Could not get socket flags. Error {}",
             isBlockingRes.errorMessage());
    return Result<>::fromError(isBlockingRes.getError());
  }

  // Cache the isBlocking value
  bool tmpIsBlocking = isBlockingRes.getValue();

  // Required for the connect call
  setBlocking(false);

  int err = ::connect(sockFd, destinationAddress.getSockAddrPtr(),
                      destinationAddress.getSockAddrLen());

  if (err < 0 && errno != EINPROGRESS) {
    // Connection failed immediately
    close();

    return Result<>::fromLastErrno();
  }

  if (timeoutMs > 0) {
    if (err < 0 && errno == EINPROGRESS) {
      // Connection is in progress; use poll to wait for completion
      struct pollfd pfd {};
      pfd.fd = sockFd;
      pfd.events = POLLOUT;

      int pollResult = ::poll(&pfd, 1, timeoutMs);
      if (pollResult <= 0) {
        // Timeout or error
        close();

        if (pollResult == 0) {
          return Result<>::fromError(std::errc::timed_out);
        }

        return Result<>::fromLastErrno();
      }

      // Check for connection success or error
      auto errCode = lastError();
      if (errCode) {
        return Result<>::fromError(errCode);
      }

      // Success
      err = 0;
    }

    // Restore isBlocking
    setBlocking(tmpIsBlocking);
  }

  return {};
}

Result<> TCPSocket::listen(int backlog) {
  if (!isValid()) {
    return Result<>::fromError(std::errc::invalid_argument);
  }

  if (::listen(sockFd, backlog) != 0) {
    return Result<>::fromLastErrno();
  }

  return {};
}

Result<TCPSocket> TCPSocket::accept() {
  struct sockaddr_in clientAddr {};
  socklen_t addrLen = sizeof(clientAddr);

  // Accept the incoming connection
  int clientFd = ::accept(
      sockFd, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

  if (clientFd < 0) {
    return Result<TCPSocket>::fromLastErrno();
  }

  return TCPSocket(clientFd);
}
