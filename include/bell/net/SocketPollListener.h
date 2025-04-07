#pragma once

// Standard includes
#include <sys/poll.h>
#include <functional>
#include <memory>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "bell/net/Socket.h"

namespace bell::net {
class SocketPollListener {
 public:
  // Default constructor
  SocketPollListener() = default;

  using EventCallback = std::function<void(Socket&)>;

  /**
   * @brief Registers a socket with the poll listener.
   *
   * @param socket Pointer to the socket to register
   * @param onWriteable writeable / POLLOUT callback
   * @param onReadable readable / POLLIN callback
   * @param onError on error / POLLERR callback
   */
  void registerSocket(const std::shared_ptr<Socket>& socket,
                      const EventCallback& onWriteable = {},
                      const EventCallback& onReadable = {},
                      const EventCallback& onError = {});

  // Polls the registered sockets for events
  void poll(int timeoutMs = 100);

  // Unregisters a socket from the poll listener
  void unregisterSocket(int fd);

 private:
  struct SocketCallbacks {
    // Weak pointer to the registered socket
    std::weak_ptr<Socket> socketPtr;

    // Callback functions for different events
    EventCallback onWriteable;
    EventCallback onReadable;
    EventCallback onError;
  };

  // keeps reference to socket we're listening to events from
  std::unordered_map<int, SocketCallbacks> handlers;
  std::vector<pollfd> fds;
};
}  // namespace bell::net

namespace bell {
using SocketPollListener = net::SocketPollListener;
}
