#include "bell/io/TCPSocket.h"

#include "bell/Logger.h"
#include "bell/io/SocketUtils.h"

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

using namespace bell::io;

TCPSocket::~TCPSocket() {
  close();
}

void TCPSocket::connect(const std::string& host, uint16_t port, int timeoutMs) {
  int err;

  // Close the socket if it is already open
  if (isOpen()) {
    close();
  }

  destinationAddress = SocketUtils::resolveDomain(host, SOCK_STREAM);
  destinationAddress.setPort(port);

  sockFd = socket(destinationAddress.family, SOCK_STREAM, IPPROTO_IP);

  if (sockFd < 0) {
    BELL_LOG(error, LOG_TAG, "Could not create socket to {}, port {}. Error {}",
             host.c_str(), port, errno);
    throw std::runtime_error("Sock create failed");
  }

  isClosed = false;

  // Required for the connect call
  setBlocking(false);

  err = ::connect(sockFd,
                  reinterpret_cast<struct sockaddr*>(&destinationAddress.addr),
                  destinationAddress.addrLen);

  if (err < 0 && errno != EINPROGRESS) {
    // Connection failed immediately
    close();
    BELL_LOG(error, LOG_TAG, "Could not connect to {}, port {}. Error {}",
             host.c_str(), port, errno);
    throw std::runtime_error("Sock connect failed");
  }

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
        BELL_LOG(error, LOG_TAG, "Connection to {} timed out after {} ms.",
                 host.c_str(), timeoutMs);
        throw std::runtime_error("Sock connect timeout");
      }

      BELL_LOG(error, LOG_TAG, "Polling error while connecting to {}. Error {}",
               host.c_str(), errno);
      throw std::runtime_error("Sock connect poll failed");
    }

    // Check for connection success or error
    int sockErr;
    socklen_t sockErrLen = sizeof(sockErr);
    if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &sockErr, &sockErrLen) < 0 ||
        sockErr != 0) {
      close();
      BELL_LOG(error, LOG_TAG, "Connection to {} failed. Socket error {}",
               host.c_str(), sockErr);
      throw std::runtime_error("Sock connect failed");
    }
  }

  // Set socket back to the requested blocking mode
  setBlocking(isBlocking);
}

std::unique_ptr<Socket> TCPSocket::accept() {
  struct sockaddr_in clientAddr {};
  socklen_t addrLen = sizeof(clientAddr);

  // Accept the incoming connection
  int clientFd = ::accept(
      sockFd, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
  if (clientFd < 0) {
    throw std::runtime_error("Socket accept failed");
  }

  // Create a new TCPSocket object for the accepted connection
  std::unique_ptr<TCPSocket> clientSocket = std::make_unique<TCPSocket>();
  clientSocket->wrapFd(
      clientFd);  // Wrap the file descriptor in a new socket object

  return clientSocket;  // Return the new socket object for the accepted connection
}