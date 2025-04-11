#include "bell/net/SocketPollListener.h"

// Standar includes
#include <sys/poll.h>
#include <cstring>

#include "fmt/format.h"

using namespace bell::net;

void SocketPollListener::registerSocket(const std::shared_ptr<Socket>& socket,
                                        Event polledEvent,
                                        const EventCallback& onEvent) {
  std::scoped_lock lock(pollMutex);

  if (!socket || !socket->isValid()) {
    throw std::invalid_argument("Invalid socket");
  }

  if (handlers.find(socket->getFd()) == handlers.end()) {
    // Create a new handler for the socket
    handlers[socket->getFd()] = {.socketPtr = socket, .callbacks = {}};
  }

  handlers[socket->getFd()].callbacks[polledEvent] = onEvent;

  updateFdList();
}
void SocketPollListener::updateFdList() {
  fds.clear();
  for (const auto& handler : handlers) {
    pollfd pfd{};
    pfd.fd = handler.first;

    pfd.events = 0;

    for (const auto& callback : handler.second.callbacks) {
      switch (callback.first) {
        case Event::Readable:
          pfd.events |= POLLIN;
          break;
        case Event::Writeable:
          pfd.events |= POLLOUT;
          break;
        case Event::Error:
          pfd.events |= POLLERR;
          break;
        case Event::Hangup:
          pfd.events |= POLLHUP;
          break;
        case Event::Priority:
          pfd.events |= POLLPRI;
          break;
      }
    }

    // Add the file descriptor to the list
    fds.push_back(pfd);
  }
}

void SocketPollListener::unregisterSocket(int fd) {
  std::scoped_lock lock(pollMutex);

  // Remove the handler
  handlers.erase(fd);

  updateFdList();
}

void SocketPollListener::poll(int timeoutMs) {
  std::scoped_lock lock(pollMutex);

  if (fds.empty()) {
    return;  // Nothing to poll
  }

  int pollResult = ::poll(fds.data(), fds.size(), timeoutMs);

  if (pollResult < 0) {  // Handle polling error
    throw std::runtime_error(
        fmt::format("poll failed err={}", strerror(errno)));
    return;
  }

  bool rebuildFdList = false;

  // Erase all FDS with expired weakptr
  for (auto it = handlers.begin(); it != handlers.end();) {
    if (it->second.socketPtr.expired()) {
      rebuildFdList = true;
      it = handlers.erase(it);
    } else {
      ++it;
    }
  }

  if (rebuildFdList) {
    updateFdList();
  }

  auto fdsCopy = fds;  // Copy the file descriptors to avoid modification
  for (auto& pfd : fdsCopy) {
    if (pfd.revents != 0) {  // If there are any events
      auto it = handlers.find(pfd.fd);
      if (it != handlers.end()) {
        auto socketPtr = it->second.socketPtr.lock();

        if (!socketPtr) {
          continue;
        }

        if ((pfd.revents & POLLIN) &&
            it->second.callbacks.contains(Event::Readable)) {
          // Call the readable callback
          it->second.callbacks[Event::Readable](*socketPtr);
        }

        if ((pfd.revents & POLLOUT) &&
            it->second.callbacks.contains(Event::Writeable)) {
          // Call the writeable callback
          it->second.callbacks[Event::Writeable](*socketPtr);
        }

        if ((pfd.revents & POLLPRI) &&
            it->second.callbacks.contains(Event::Priority)) {
          // Call the priority callback
          it->second.callbacks[Event::Priority](*socketPtr);
        }

        if ((pfd.revents & POLLERR) &&
            it->second.callbacks.contains(Event::Error)) {
          // Call the writeable callback
          it->second.callbacks[Event::Writeable](*socketPtr);
        }

        if (pfd.revents & POLLHUP) {
          if (it->second.callbacks.contains(Event::Hangup)) {
            // Call the hangup callback
            it->second.callbacks[Event::Hangup](*socketPtr);
          }

          // Remove the handler
          handlers.erase(it->first);

          // Rebuild the file descriptor list
          updateFdList();
        }
      }
    }
  }
}
