#include "bell/net/SocketPollListener.h"

// Standar includes
#include <sys/poll.h>
#include <algorithm>
#include <cstring>

#include "fmt/format.h"

using namespace bell::net;

void SocketPollListener::registerSocket(const std::shared_ptr<Socket>& socket,
                                        const EventCallback& onWriteable,
                                        const EventCallback& onReadable,
                                        const EventCallback& onError) {
  SocketCallbacks callbacks = {.socketPtr = socket,
                               .onWriteable = onWriteable,
                               .onReadable = onReadable,
                               .onError = onError};

  handlers.insert({socket->getFd(), callbacks});

  // Add to the file descriptor list for polling
  pollfd pfd{};
  pfd.fd = socket->getFd(), pfd.events = POLLIN | POLLOUT | POLLERR;
  fds.push_back(pfd);
}

void SocketPollListener::unregisterSocket(int fd) {
  // Remove the handler
  handlers.erase(fd);

  // Remove the file descriptor from the list
  auto it =
      std::find_if(fds.begin(), fds.end(),
                   [fd](const struct pollfd& pfd) { return pfd.fd == fd; });

  if (it != fds.end()) {
    fds.erase(it);
  }
}

void SocketPollListener::poll(int timeoutMs) {
  if (fds.empty()) {
    return;  // Nothing to poll
  }

  int pollResult = ::poll(fds.data(), fds.size(), timeoutMs);

  if (pollResult < 0) {  // Handle polling error
    throw std::runtime_error(
        fmt::format("poll failed err={}", strerror(errno)));
    return;
  }

  // Erase all FDS with expired weakptr
  for (auto it = handlers.begin(); it != handlers.end();) {
    if (it->second.socketPtr.expired()) {

      // Remove the file
      auto invalidFdItr = std::find_if(
          fds.begin(), fds.end(),
          [it](const struct pollfd& pfd) { return pfd.fd == it->first; });

      if (invalidFdItr != fds.end()) {
        fds.erase(invalidFdItr);
      }

      it = handlers.erase(it);
    } else {
      ++it;
    }
  }

  auto fdsCopy = fds;  // Copy the file descriptors to avoid modification
  for (auto& pfd : fdsCopy) {
    if (pfd.revents != 0) {  // If there are any events
      auto it = handlers.find(pfd.fd);
      if (it != handlers.end()) {
        if (pfd.revents & POLLIN) {
          // Call the readable callback
          auto socketPtr = it->second.socketPtr.lock();
          if (socketPtr) {
            it->second.onReadable(*socketPtr);
          }
        }

        if (pfd.revents & POLLOUT) {
          // Call the readable callback
          auto socketPtr = it->second.socketPtr.lock();
          if (socketPtr) {
            it->second.onWriteable(*socketPtr);
          }
        }

        if (pfd.revents & POLLERR) {
          // Call the readable callback
          auto socketPtr = it->second.socketPtr.lock();
          if (socketPtr) {
            it->second.onError(*socketPtr);
          }
        }

        if (pfd.revents & POLLHUP) {
          // Remove the handler
          unregisterSocket(it->first);
        }
      }
    }
  }
}
