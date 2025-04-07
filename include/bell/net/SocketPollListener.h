#pragma once

// Standard includes
#include <sys/poll.h>
#include <functional>
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
  using ErrorCallback = std::function<void(Socket&, std::error_code)>;

  /**
   * @brief Registers a socket with the poll listener.
   *
   * @param fd File descriptor of the socket
   * @param eventMask Bitmask of events to listen for, e.g., POLLIN, POLLOUT
   * @param eventCallback Callback function to call when the event occurs
   */
  void pollSocket(std::shared_ptr<Socket> socket,
                  const EventCallback& onWriteable = {},
                  const EventCallback& onReadable = {},
                  const EventCallback& onError = {});

  // Polls the registered sockets for events
  void poll(int timeoutMs = -1);

  // Unregisters a socket from the poll listener
  void unregisterSocket(int fd);

 private:
  struct SocketCallbacks {
    // Weak pointer to the registered socket
    std::weak_ptr<Socket> socketPtr;

    // Callback functions for different events
    EventCallback onWriteable;
    EventCallback onReadable;
    ErrorCallback onError;
  };

  // keeps reference to socket we're listening to events from
  std::unordered_map<int, SocketCallbacks> handlers;
  std::vector<pollfd> events;
};
}  // namespace bell::net

namespace bell {
using SocketPollListener = net::SocketPollListener;
}
