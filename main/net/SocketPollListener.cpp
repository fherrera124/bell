#include "bell/net/SocketPollListener.h"

// System includes
#include <sys/poll.h>
#include <algorithm>
#include <cstring>

#include "fmt/format.h"

using namespace bell::net;

void SocketPollListener::registerSocket(
    int fd, short eventMask, const SocketEventCallback& eventCallback) {
  handlers.insert({fd, eventCallback});

  // Add to the file descriptor list for polling
  struct pollfd pfd {};
  pfd.fd = fd;
  pfd.events = eventMask;

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

  for (auto& pfd : fds) {
    if (pfd.revents != 0) {  // If there are any events
      auto it = handlers.find(pfd.fd);
      if (it != handlers.end()) {
        // Invoke the callback with the specific revents bitmask
        it->second(pfd.revents);

        if (pfd.revents & POLLHUP) {
          // Remove the handler
          handlers.erase(it);
        }
      }
    }
  }
}
